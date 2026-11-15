#include <esp_random.h>
#include <esp_timer.h>
#include <esp_heap_caps.h>
#include <esp_event.h>
#include <esp_netif.h>
#include <esp_wifi.h>
#include <nvs_flash.h>
#include <esp_http_server.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>
#include <string>
#include <vector>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "llm.h"
#include "web_page.h"

extern "C" const uint8_t MODEL_DATA[];
extern "C" const size_t MODEL_DATA_LEN;
extern "C" const uint8_t TOKENIZER_DATA[];
extern "C" const size_t TOKENIZER_DATA_LEN;

static Transformer model;
static Tokenizer tokenizer;
static Sampler sampler;
static SemaphoreHandle_t chat_mutex;
struct Turn { std::string user, bot; };
static std::vector<Turn> history;
static constexpr int KV_WINDOW = 72;
static constexpr int REPLY_LIMIT = 24;

struct ChatResult { std::string text; int prompt_tokens=0, generated=0; int64_t prefill_ms=0, decode_ms=0; bool ok=false, calculator=false; };

static bool try_calculate(const std::string& input, std::string& answer) {
  const char* p = input.c_str();
  while (*p && !(*p >= '0' && *p <= '9')) ++p;
  if (!*p) return false;
  char* end = nullptr;
  long long a = strtoll(p, &end, 10);
  if (end == p || a > 1000000 || a < -1000000) return false;
  p = end;
  while (*p == ' ' || *p == '\t') ++p;
  char op = *p++;
  if (op != '+' && op != '-' && op != '*' && op != '/' && op != 'x' && op != 'X') return false;
  while (*p == ' ' || *p == '\t') ++p;
  long long b = strtoll(p, &end, 10);
  if (end == p || b > 1000000 || b < -1000000) return false;
  p = end;
  while (*p == ' ' || *p == '\t' || *p == '?' || *p == '.' || *p == '!') ++p;
  if (*p) return false;
  long long value = 0;
  switch (op) {
    case '+': value = a + b; break;
    case '-': value = a - b; break;
    case '*': case 'x': case 'X': value = a * b; break;
    case '/': if (!b || a % b) return false; value = a / b; break;
    default: return false;
  }
  char buf[48];
  snprintf(buf, sizeof(buf), "%lld", value);
  answer = buf;
  return true;
}

static ChatResult chat(const std::string& user) {
  ChatResult result;
  if (try_calculate(user, result.text)) {
    result.ok = true;
    result.calculator = true;
    return result;
  }
  std::vector<std::string> parts;
  // The model has no native system-role token. Keep this compact instruction
  // as a short opening exchange in its trained User/Bot format.
  parts.push_back("User: Answer me directly, naturally, and on topic. Don't repeat old replies. If unsure, say so.\nBot: Okay.");
  for (const auto& turn : history) parts.push_back("User: " + turn.user + "\nBot:" + turn.bot);
  parts.push_back("User: " + user + "\nBot:");
  int prompt[KV_WINDOW - 2], n = 0;
  bool fit = false;
  while (!parts.empty() && !fit) {
    n = 0; fit = true;
    for (size_t i = 0; i < parts.size(); ++i) {
      int segment[200], count = 0;
      if (parts[i].size() + 8 > 200) { fit = false; break; }
      llm_encode(&tokenizer, parts[i].c_str(), 0, 0, segment, &count);
      if (n + count + (i ? 2 : 0) > KV_WINDOW - REPLY_LIMIT - 2) { fit = false; break; }
      if (i) {
        prompt[n++] = tokenizer.eos_id;
        int nl[8], nl_count = 0;
        llm_encode(&tokenizer, "\n", 0, 0, nl, &nl_count);
        for (int j = 0; j < nl_count; ++j) prompt[n++] = nl[j];
      }
      for (int j = 0; j < count; ++j) prompt[n++] = segment[j];
    }
    // Preserve the instruction and current message; evict oldest chat turns.
    if (!fit && parts.size() > 2) parts.erase(parts.begin() + 1);
    else if (!fit) break;
  }
  if (!fit || n < 1) { result.text = "Message is too long for the model context."; return result; }
  result.prompt_tokens = n;
  int token = prompt[0], pos = 0;
  const int64_t total_start_us = esp_timer_get_time();
  int64_t decode_start_us = 0;
  while (pos < KV_WINDOW && result.generated < REPLY_LIMIT) {
    float* logits = llm_forward(&model, token, pos);
    if (pos < n - 1) { token = prompt[pos + 1]; ++pos; continue; }
    if (!decode_start_us) {
      result.prefill_ms = (esp_timer_get_time() - total_start_us) / 1000;
      decode_start_us = esp_timer_get_time();
    }
    int next = llm_sample(&sampler, logits);
    if (next == tokenizer.eos_id) break;
    char scratch[32];
    const char* piece = llm_decode(&tokenizer, token, next, scratch, sizeof(scratch));
    if (piece && *piece) result.text += piece;
    token = next; ++pos; ++result.generated;
  }
  result.decode_ms = decode_start_us ? (esp_timer_get_time() - decode_start_us) / 1000 : 0;
  result.ok = true;
  if (!result.text.empty()) { history.push_back({user, result.text}); if (history.size() > 4) history.erase(history.begin()); }
  return result;
}

static esp_err_t page_handler(httpd_req_t* req) {
  httpd_resp_set_type(req, "text/html; charset=utf-8");
  httpd_resp_set_hdr(req, "Cache-Control", "no-store");
  return httpd_resp_send(req, WEB_PAGE, HTTPD_RESP_USE_STRLEN);
}
static esp_err_t health_handler(httpd_req_t* req) {
  const char* body = "{\"device\":\"XIAO ESP32-S3\",\"model\":\"TinyTalk-2\",\"offline\":true}";
  httpd_resp_set_type(req, "application/json");
  return httpd_resp_send(req, body, HTTPD_RESP_USE_STRLEN);
}
static esp_err_t reset_handler(httpd_req_t* req) {
  if (xSemaphoreTake(chat_mutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
    httpd_resp_set_status(req, "503 Service Unavailable");
    return httpd_resp_sendstr(req, "The model is busy. Try clearing the chat again shortly.");
  }
  history.clear();
  xSemaphoreGive(chat_mutex);
  return httpd_resp_sendstr(req, "Conversation cleared.");
}
static esp_err_t chat_handler(httpd_req_t* req) {
  if (req->content_len < 1 || req->content_len > 180) {
    httpd_resp_set_status(req, "413 Payload Too Large");
    return httpd_resp_sendstr(req, "Message must be 1 to 180 bytes.");
  }
  if (xSemaphoreTake(chat_mutex, 0) != pdTRUE) {
    httpd_resp_set_status(req, "503 Service Unavailable");
    return httpd_resp_sendstr(req, "The model is already generating a reply. Try again shortly.");
  }
  std::string user;
  user.resize(req->content_len);
  int received = 0;
  while (received < req->content_len) {
    int n = httpd_req_recv(req, &user[received], req->content_len - received);
    if (n <= 0) { xSemaphoreGive(chat_mutex); return ESP_FAIL; }
    received += n;
  }
  while (!user.empty() && (user.back() == '\n' || user.back() == '\r')) user.pop_back();
  if (user.empty()) { xSemaphoreGive(chat_mutex); httpd_resp_set_status(req, "400 Bad Request"); return httpd_resp_sendstr(req, "Type a message first."); }
  ChatResult r = chat(user);
  // httpd_resp_set_hdr may retain the value pointer until the response is sent.
  // Keep each header value in its own buffer instead of overwriting one shared buffer.
  char prompt_tokens[16], generated_tokens[16], prefill_ms[24], decode_ms[24];
  snprintf(prompt_tokens, sizeof(prompt_tokens), "%d", r.prompt_tokens);
  snprintf(generated_tokens, sizeof(generated_tokens), "%d", r.generated);
  snprintf(prefill_ms, sizeof(prefill_ms), "%lld", (long long)r.prefill_ms);
  snprintf(decode_ms, sizeof(decode_ms), "%lld", (long long)r.decode_ms);
  httpd_resp_set_hdr(req, "X-Reply-Source", r.calculator ? "calculator" : "model");
  httpd_resp_set_hdr(req, "X-Prompt-Tokens", prompt_tokens);
  httpd_resp_set_hdr(req, "X-Generated-Tokens", generated_tokens);
  httpd_resp_set_hdr(req, "X-Prefill-Ms", prefill_ms);
  httpd_resp_set_hdr(req, "X-Decode-Ms", decode_ms);
  httpd_resp_set_type(req, "text/plain; charset=utf-8");
  esp_err_t err = httpd_resp_send(req, r.text.c_str(), r.text.size());
  xSemaphoreGive(chat_mutex);
  return err;
}
static void start_web_server() {
  httpd_config_t cfg = HTTPD_DEFAULT_CONFIG();
  cfg.server_port = 80;
  cfg.max_uri_handlers = 4;
  cfg.max_open_sockets = 3;
  cfg.stack_size = 8192;
  httpd_handle_t server = nullptr;
  ESP_ERROR_CHECK(httpd_start(&server, &cfg));
  httpd_uri_t page = {.uri="/", .method=HTTP_GET, .handler=page_handler, .user_ctx=nullptr};
  httpd_uri_t health = {.uri="/api/health", .method=HTTP_GET, .handler=health_handler, .user_ctx=nullptr};
  httpd_uri_t reset = {.uri="/api/reset", .method=HTTP_POST, .handler=reset_handler, .user_ctx=nullptr};
  httpd_uri_t chat_uri = {.uri="/api/chat", .method=HTTP_POST, .handler=chat_handler, .user_ctx=nullptr};
  ESP_ERROR_CHECK(httpd_register_uri_handler(server, &page));
  ESP_ERROR_CHECK(httpd_register_uri_handler(server, &health));
  ESP_ERROR_CHECK(httpd_register_uri_handler(server, &reset));
  ESP_ERROR_CHECK(httpd_register_uri_handler(server, &chat_uri));
}
static void start_access_point() {
  ESP_ERROR_CHECK(nvs_flash_init());
  ESP_ERROR_CHECK(esp_netif_init());
  ESP_ERROR_CHECK(esp_event_loop_create_default());
  esp_netif_create_default_wifi_ap();
  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  ESP_ERROR_CHECK(esp_wifi_init(&cfg));
  wifi_config_t ap = {};
  strcpy((char*)ap.ap.ssid, "TinyTalk-XIAO");
  strcpy((char*)ap.ap.password, "needlechat");
  ap.ap.ssid_len = strlen("TinyTalk-XIAO");
  ap.ap.channel = 1;
  ap.ap.max_connection = 2;
  ap.ap.authmode = WIFI_AUTH_WPA2_PSK;
  ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
  ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &ap));
  ESP_ERROR_CHECK(esp_wifi_start());
}

static void serial_terminal_task(void*) {
  std::string line;
  printf("\nUSB terminal ready. Type a message; /new clears conversation.\n> ");
  for (;;) {
    int c = getchar();
    if (c == EOF) { vTaskDelay(pdMS_TO_TICKS(10)); continue; }
    if (c == '\r') continue;
    if (c == '\n') {
      putchar('\n');
      if (line == "/new") {
        if (xSemaphoreTake(chat_mutex, portMAX_DELAY) == pdTRUE) {
          history.clear(); xSemaphoreGive(chat_mutex);
        }
        printf("Conversation cleared.\n> ");
      } else if (!line.empty()) {
        if (xSemaphoreTake(chat_mutex, portMAX_DELAY) == pdTRUE) {
          ChatResult r = chat(line);
          printf("@TT2:BEGIN|%d\nNEEDLE> %s\n@TT2:END|%d|%lld|%lld\n",
                 r.prompt_tokens, r.text.c_str(), r.generated,
                 (long long)r.prefill_ms, (long long)r.decode_ms);
          xSemaphoreGive(chat_mutex);
          printf("> ");
        }
      }
      line.clear();
    } else if (c == 8 || c == 127) {
      if (!line.empty()) { line.pop_back(); fputs("\b \b", stdout); }
    } else if (c >= 32 && c < 127 && line.size() < 180) {
      line.push_back((char)c); putchar(c);
    }
  }
}

extern "C" void app_main(void) {
  printf("\nTinyTalk-2 XIAO ESP32-S3 offline web chat\n");
  printf("Initializing embedded model (%u KB)...\n", (unsigned)(MODEL_DATA_LEN / 1024));
  if (!llm_init_embedded(&model, MODEL_DATA, MODEL_DATA_LEN, KV_WINDOW)) { printf("ERROR: model init failed\n"); vTaskDelete(nullptr); return; }
  if (!llm_tokenizer_from_memory(&tokenizer, TOKENIZER_DATA, TOKENIZER_DATA_LEN, model.config.vocab_size)) { printf("ERROR: tokenizer init failed\n"); vTaskDelete(nullptr); return; }
  if (tokenizer.vocab_size != model.config.vocab_size) { printf("ERROR: model/tokenizer vocab mismatch\n"); vTaskDelete(nullptr); return; }
  // A lower temperature keeps this tiny chat model more focused than the
  // upstream demo's more varied 0.8 setting while retaining top-p sampling.
  llm_build_sampler(&sampler, model.config.vocab_size, 0.2f, 0.9f, esp_random());
  chat_mutex = xSemaphoreCreateMutex();
  start_access_point();
  start_web_server();
  printf("Wi-Fi: TinyTalk-XIAO / password: needlechat\nOpen: http://192.168.4.1/\n");
  printf("Internal free heap: %u bytes; PSRAM free: %u bytes\n", (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL), (unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
  xTaskCreate(serial_terminal_task, "tiny_serial", 6144, nullptr, 2, nullptr);
  for (;;) vTaskDelay(pdMS_TO_TICKS(1000));
}

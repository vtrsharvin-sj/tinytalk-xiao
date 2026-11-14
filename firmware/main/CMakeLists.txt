idf_component_register(
    SRCS main.cpp llm.cpp dot_q4_pie.S model_data.cpp tok_data.cpp
    INCLUDE_DIRS .
    PRIV_REQUIRES esp_timer driver esp_wifi esp_event esp_netif nvs_flash esp_http_server
)

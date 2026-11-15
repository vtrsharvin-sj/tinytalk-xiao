# Model terms and attribution

The embedded model starts from **TheREZOR/TinyTalk**, licensed CC BY 4.0. This firmware contains a fine-tuned derivative converted to Q4_0 with a pruned GPT-2 tokenizer.

- Model card and required attribution: [TheREZOR/TinyTalk](https://huggingface.co/TheREZOR/TinyTalk)
- Base model license: [CC BY 4.0](https://creativecommons.org/licenses/by/4.0/)
- Changes: fine-tuned on 4,096 short prompts and generated replies; vocabulary-pruned, Q4_0 quantized, and embedded in firmware.

## Fine-tuning data and teacher

- Style prompts came from [aarohanverma/simple-daily-conversations-cleaned](https://huggingface.co/datasets/aarohanverma/simple-daily-conversations-cleaned), licensed Apache-2.0. The dataset has single snippets rather than prompt/reply pairs; 4,096 snippets were used as user prompts, and the source dataset itself is not redistributed.
- Replies were generated offline with [Qwen2.5-3B-Instruct](https://huggingface.co/Qwen/Qwen2.5-3B-Instruct). Its model card identifies the license as Qwen Research License, not Apache-2.0. The teacher weights are not included. The license and required attribution notice are included in this repository; the license limits use of Qwen materials and derivatives to non-commercial purposes.
- GPT-2 byte-level BPE tokenizer data is based on OpenAI's GPT-2 release (MIT) and pruned for the embedded model.

The project source code remains MIT-licensed. The embedded model and generated teacher data carry their respective upstream terms; see `NOTICE` and `QWEN-RESEARCH-LICENSE.txt`.

#Para Clonar Use: 
---
    git clone --recurse-submodules https://github.com/phkaiser13/TrackieLLM.git
    git submodule update --remote
---
---
Justificativa base:

TrackieLLM é a versão de mais arquitetura do trackie.
visa dispositivos embarcados.
A estrutura de desenvolvimento é obrigatoriamente desenvolvida em c, com pontos em assembly e pontos em rust.

Assembly > Escrito em padrão ARM, faz pontes e redirecionamentos pro sistema, abre portas e salva ponteiros.
TrackieOS> Baseado no kernel linux para arm, ocupa de 30-70mb de memória ram.

Ponto alvo> Orange pi 3-5 < é obrigatório um modelo com pelo menos 8gb de ram, a orange toma vantagem sobre a raspberry pela integração com cuda.

'''
---


LLM>>
---
LLAMA.cpp
Gemma
pytorch
onnx runtime

Mais detalhamento será descrito em TrackieLLM.md

'''
---

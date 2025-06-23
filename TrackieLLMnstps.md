Dividiremos o trabalho em fases, assim como em um projeto de software real.

---

### **Fase 0: Fundação e Ferramental (O Alicerce)**

**Objetivo:** Preparar o ambiente e as ferramentas para que o desenvolvimento seja automatizado e confiável desde o primeiro dia. Não escrevemos código de feature aqui, apenas o que é necessário para construir e testar.

**Tarefas:**

1.  **Inicializar o Repositório Git Corretamente:**
    *   **O que fazer:**
        1.  `git init` no diretório raiz.
        2.  Criar o arquivo `.gitignore` com as entradas que definimos (ex: `build/`, `*.o`, `__pycache__/`, etc.).
        3.  Adicionar os submódulos:
            ```bash
            git submodule add https://github.com/ggerganov/llama.cpp.git third_party/llama.cpp
            git submodule add https://github.com/microsoft/onnxruntime.git third_party/onnxruntime
            ```
        4.  Fazer o primeiro commit: `git commit -m "Initial project structure and submodules"`.
    *   **Entregável:** Um repositório Git limpo com a estrutura de diretórios e submódulos prontos.

2.  **Implementar o Pipeline de CI/CD (`ci_cd.yml`):**
    *   **O que fazer:** Implementar o workflow do GitHub Actions. Ele deve, desde o início:
        1.  Rodar em `push` e `pull_request` para o branch `main`.
        2.  Usar uma matriz de build (`strategy: matrix`) para rodar em `ubuntu-latest` e `windows-latest`.
        3.  **No job do Linux:**
            *   Fazer checkout do código (com submódulos).
            *   Executar `scripts/setup/install_deps.sh`.
            *   Executar `make build`.
            *   Executar `make test` (vai falhar no início, mas o passo deve existir).
        4.  **No job do Windows:**
            *   Fazer checkout do código.
            *   Executar `scripts/setup/install_deps.bat`.
            *   Executar os comandos do CMake para build (ex: `cmake .. && cmake --build .`).
            *   Executar `ctest`.
    *   **Entregável:** Um arquivo `ci_cd.yml` funcional. O build passará, mas os testes falharão (ou não encontrarão testes), o que é esperado.

3.  **Configurar o Framework de Testes:**
    *   **O que fazer:**
        1.  Criar o diretório `tests/` na raiz.
        2.  Adicionar o Google Test como um submódulo em `tests/lib/googletest`.
        3.  Criar um `tests/CMakeLists.txt` que configure o Google Test e adicione um subdiretório para testes unitários.
        4.  Criar `tests/unit/CMakeLists.txt`.
        5.  Escrever **um único teste dummy** em `tests/unit/core_test.cpp` que simplesmente faz `EXPECT_EQ(1, 1);`.
    *   **Entregável:** O CI/CD agora deve passar completamente (build e teste), pois encontrou e executou um teste que passa.

---

### **Fase 1: O "Esqueleto Andante" (A Primeira Execução)**

**Objetivo:** Ter uma versão mínima do `trackiellm.exe` que executa, passa dados de ponta a ponta (mesmo que falsos) e desliga de forma limpa. Isso prova que a arquitetura principal (módulos, threads, HAL) funciona.

**Tarefas:**

1.  **Implementar a HAL com Stubs:**
    *   **O que fazer:** Não vamos usar hardware real ainda. Crie `modules/hal-c/src/camera_stub.c` e `audio_stub.c`.
        *   `hal_camera_open`: Aloca a struct `hal_camera_t`, não faz mais nada.
        *   `hal_camera_grab_frame`: **Não** acessa a webcam. Em vez disso, aloca um buffer de memória e o preenche com um padrão de cor simples (ex: um gradiente de vermelho para azul). Isso nos dá dados de imagem previsíveis para testar.
        *   `hal_camera_release_frame`: Libera a memória do frame falso.
        *   Todas as outras funções da HAL retornam `HAL_STATUS_OK` ou `HAL_STATUS_NOT_SUPPORTED` sem fazer nada.
    *   **Entregável:** Uma biblioteca `via_hal` que compila e "funciona" sem hardware.

2.  **Implementar o Esqueleto do `PerceptionEngine`:**
    *   **O que fazer:** No `PerceptionEngine.cpp`:
        *   `initialize`: Abre a câmera usando `hal_camera_open`.
        *   `perceptionLoop`: Entra em um loop. Dentro do loop:
            1.  Chama `hal_camera_grab_frame` para obter o frame falso.
            2.  **Não** executa nenhum processador de IA ainda.
            3.  Imprime no console: `[PerceptionEngine] Frame #123 grabbed with size 640x480.`.
            4.  Chama `hal_camera_release_frame`.
            5.  Dorme por 33ms (para simular ~30 FPS).
    *   **Entregável:** Uma biblioteca `via_perception` que pode ser iniciada e parada, e que imprime logs em sua própria thread.

3.  **Implementar o `App` e o `main`:**
    *   **O que fazer:** Implementar o `App.cpp` e `main.cpp` conforme projetado. O `App` irá:
        1.  Carregar uma configuração falsa (não precisa do `config_loader-rs` ainda).
        2.  Inicializar a HAL (com os stubs).
        3.  Instanciar e inicializar o `PerceptionEngine`.
        4.  Iniciar o `PerceptionEngine`.
        5.  Entrar no loop `while(s_isRunning)` no `main thread`.
        6.  No Ctrl+C, chamar `shutdown()`, que por sua vez chama `stop()` no `PerceptionEngine` e desliga a HAL.
    *   **Entregável:** Um executável `trackiellm` que roda, mostra os logs do `PerceptionEngine`, e fecha graciosamente com Ctrl+C.

**Ao final da Fase 1, temos um programa que prova que o ciclo de vida do módulo, o threading e a interação com a HAL funcionam.**

---

### **Fase 2: Implementação das Features Centrais**

**Objetivo:** Fazer cada módulo principal funcionar de verdade, um de cada vez.

**Contexto que eu preciso de você:** Qual é o fluxo de feature mais importante para validar primeiro? Proponho a seguinte ordem, mas podemos ajustar: **Detecção de Objetos -> Descrição por LLM -> Saída de Voz.**

**Tarefas (assumindo a ordem proposta):**

1.  **Feature: Detecção de Objetos:**
    *   **Rust:** Implementar o `config_loader-rs` (`lib.rs`, `build.rs`, `Cargo.toml`) para que possamos carregar o caminho do modelo YOLO do `hardware.default.yml`.
    *   **Perception:** Implementar o `OnnxRuntimeProcessor.cpp`. Ele deve carregar o `yolo_v8n.onnx`, implementar o pré-processamento (resize, normalize) e o pós-processamento (NMS) em C++ puro por enquanto.
    *   **Perception:** Modificar o `PerceptionEngine` para instanciar e executar o `OnnxRuntimeProcessor` em cada frame.
    *   **Core/Shared:** Definir a `struct BoundingBox` e o `struct SceneData`. O `PerceptionEngine` agora preenche o `SceneData` com os `BoundingBox`es detectados.
    *   **Core:** O `App`/`Orchestrator` recebe o `SceneData` e imprime no console: `Detected: car (0.92), person (0.88)`.
    *   **Entregável:** A aplicação agora detecta objetos no frame falso (gradiente) e imprime os resultados.

2.  **Feature: Raciocínio com LLM:**
    *   **Reasoning:** Implementar o `LlmInterpreter.cpp` para carregar o `gemma-2b-it.gguf` usando `llama.cpp`.
    *   **Core:** Modificar o `App`/`Orchestrator`. Agora, em vez de apenas imprimir as detecções, ele formata um prompt: `prompt = "I see the following objects: car, person. Describe the scene."`.
    *   **Core:** O `Orchestrator` chama `LlmInterpreter::submitPrompt(prompt)`.
    *   **Core:** Quando o `future` do LLM estiver pronto, o `Orchestrator` obtém a resposta e a imprime no console: `LLM Response: "The scene contains a car and a person."`.
    *   **Entregável:** A aplicação agora forma um prompt com base na visão, o envia para o LLM e imprime a resposta.

3.  **Feature: Saída de Voz (Text-to-Speech - TTS):**
    *   **HAL:** Implementar a parte de `hal_audio_playback` no `audio_stub.c` (pode simplesmente não fazer nada ou imprimir "Playing audio...").
    *   **Audio Module (Novo):** Criar um novo módulo `audio/` com um `AudioEngine.h/cpp` que implementa `IModule`.
    *   **Audio Module:** O `AudioEngine` usa uma biblioteca TTS leve (como `eSpeak-NG` ou Piper) para converter texto em um buffer de áudio PCM.
    *   **Core:** O `Orchestrator`, ao receber a resposta do LLM, em vez de imprimi-la, envia um comando para o `AudioEngine` para falar o texto.
    *   **Entregável:** A aplicação agora fala a resposta do LLM (ou pelo menos tenta, através da HAL de áudio).

---

### **Fase 3: Validação e Otimização no Hardware Alvo**

**Objetivo:** Fazer o sistema funcionar bem no Raspberry Pi.

**Tarefas:**

1.  **Trocar Stubs por Implementações Reais:**
    *   **O que fazer:** Implementar o `camera_linux.c` (usando V4L2) e o `audio_linux.c` (usando ALSA).
    *   **Entregável:** A aplicação agora usa a câmera e o sistema de som reais do Raspberry Pi.

2.  **Profiling:**
    *   **O que fazer:** Compilar em modo `RelWithDebInfo`. Executar a aplicação no Pi sob a ferramenta `perf`.
        ```bash
        perf record -g ./trackiellm
        perf report
        ```
    *   **Entregável:** Um relatório do `perf` identificando as 3-5 funções que mais consomem tempo de CPU. Provavelmente serão as de pré e pós-processamento de imagem.

3.  **Implementar Otimizações em Assembly:**
    *   **O que fazer:** Agora que sabemos onde estão os gargalos, implementamos os arquivos `.s` que projetamos: `preprocess_image_arm.s` e `postprocess_detect_arm.s`.
    *   **Entregável:** Código Assembly NEON que substitui as implementações lentas em C++.

4.  **Benchmark:**
    *   **O que fazer:** Medir o FPS (Frames Per Second) do pipeline de percepção antes e depois da otimização em Assembly.
    *   **Entregável:** Prova quantitativa de que a otimização funcionou (ex: "FPS aumentou de 8 para 25").

---

### **Fase 4: Endurecimento e Empacotamento**

**Objetivo:** Transformar o protótipo funcional em um produto robusto e distribuível.

**Tarefas:**

1.  **Logging Robusto:**
    *   **O que fazer:** Substituir todos os `std::cout` e `printf` por uma biblioteca de logging estruturado (como `spdlog`). Configurar o nível de log a partir do `system.default.yml`.
    *   **Entregável:** Logs consistentes, formatados e com níveis de severidade.

2.  **Tratamento de Erros Completo:**
    *   **O que fazer:** Envolver todas as chamadas de API que podem falhar (HAL, inferência de modelo, I/O de arquivo) em blocos `try-catch` (C++) ou tratar `Result`s (Rust) de forma apropriada, registrando erros detalhados.
    *   **Entregável:** A aplicação não "crasha" se a câmera for desconectada ou um modelo não for encontrado; ela registra o erro e tenta se recuperar ou desliga de forma limpa.

3.  **Finalizar Scripts de Implantação:**
    *   **O que fazer:** Testar e refinar os scripts `package_os.sh` e `package_studio.bat`. Criar um pacote `.tar.gz` e um `.zip` e tentar executá-los em uma máquina limpa.
    *   **Entregável:** Pacotes de implantação que funcionam "out-of-the-box".

4.  **Documentação Final:**
    *   **O que fazer:** Escrever o `README.md` principal com instruções claras de compilação e execução para um novo desenvolvedor.
    *   **Entregável:** Documentação que permite que outra pessoa contribua ou use o projeto.

Este plano de ação transforma o design em um roteiro de desenvolvimento iterativo e profissional. Cada fase tem um objetivo claro e produz um resultado tangível. Comecemos pela Fase 0.

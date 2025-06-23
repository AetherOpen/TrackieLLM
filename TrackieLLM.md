# TrackieLLM: Arquitetura e Documentação Técnica

Este documento detalha a arquitetura de software do TrackieLLM, um assistente inteligente para pessoas com deficiência visual, projetado para rodar em dispositivos embarcados como o Raspberry Pi e em ambientes de desenvolvimento Windows.




---

    TrackieLLM_v2/
    ├── .github/
    │   └── workflows/
    │       └── ci_cd.yml               # -> Pipeline unificado para build, teste e deploy.
    ├── assets/
    │   ├── models/                     # -> Modelos ONNX/GGUF, gerenciados por scripts.
    │   └── sounds/
    ├── build/                          # -> (Ignorado pelo Git) Diretório de saída dos builds.
    ├── config/
    │   ├── system.default.yml          # -> Configurações gerais (threads, log level).
    │   ├── hardware.default.yml        # -> Configurações de hardware (resolução da câmera, ID do microfone).
    │   └── profiles/
    │       └── joao.default.yml        # -> Exemplo de perfil de usuário do ATAD.
    ├── docs/
    │   ├── Architecture_v2.md          # -> Este documento.
    │   └── C_ABI_Reference.md          # -> Documentação da interface C da HAL e do Config Loader.
    ├── modules/
    │   ├── core/                       # Módulo Principal (Orquestrador) - C++
    │   │   ├── include/via/core/
    │   │   │   ├── App.h
    │   │   │   └── IModule.h           # -> Interface base para todos os módulos.
    │   │   ├── src/
    │   │   │   └── App.cpp
    │   │   └── CMakeLists.txt
    │   │
    │   ├── hal-c/                      # Hardware Abstraction Layer - C, com pontos em Assembly
    │   │   ├── include/via/hal/
    │   │   │   └── hal.h               # -> Interface C pura (ex: hal_camera_open(), hal_audio_capture()).
    │   │   ├── src/
    │   │   │   ├── camera.c
    │   │   │   ├── audio.c
    │   │   │   └── optim/
    │   │   │       ├── audio_filter_arm.s # -> Ponto de otimização #1: Filtro de ruído em Assembly.
    │   │   │       └── ...
    │   │   └── CMakeLists.txt
    │   │
    │   ├── config_loader-rs/           # Parser de Configuração Seguro - Rust
    │   │   ├── include/                # -> Cabeçalho C gerado para interoperabilidade.
    │   │   │   └── via_config.h
    │   │   ├── src/
    │   │   │   ├── lib.rs              # -> Lógica Rust usando serde para parsear YMLs.
    │   │   │   └── models.rs           # -> Structs Rust que mapeiam os YMLs.
    │   │   ├── Cargo.toml
    │   │   └── build.rs                # -> Gera o cabeçalho C via_config.h.
    │   │
    │   ├── perception/                 # Módulo de Percepção (ONNX) - C++, com pontos em Assembly
    │   │   ├── include/via/perception/
    │   │   │   ├── PerceptionEngine.h
    │   │   │   └── IProcessor.h
    │   │   ├── src/
    │   │   │   ├── OnnxRuntimeProcessor.cpp
    │   │   │   ├── Preprocessor.cpp    # -> Fallback em C++ para pré-processamento.
    │   │   │   └── optim/
    │   │   │       ├── preprocess_image_arm.s # -> Ponto #2: Pré-processamento de imagem (resize/normalize).
    │   │   │       └── postprocess_detect_arm.s # -> Ponto #3: Non-Maximum Suppression para detecção.
    │   │   └── CMakeLists.txt
    │   │
    │   ├── reasoning/                  # Módulo de Raciocínio (LLM) - C++
    │   │   ├── include/via/reasoning/
    │   │   │   └── LlmInterpreter.h
    │   │   ├── src/
    │   │   │   └── LlmInterpreter.cpp
    │   │   └── CMakeLists.txt
    │   │
    │   └── shared/                     # Utilitários e Estruturas de Dados Comuns - C++
    │       ├── include/via/shared/
    │       │   ├── SafeQueue.h
    │       │   └── DataStructures.h    # -> (ex: BoundingBox, SceneData).
    │       └── CMakeLists.txt
    │
    ├── scripts/
    │   ├── setup/                      # Scripts para configurar o ambiente.
    │   │   ├── install_deps.sh         # -> Instala dependências no Linux/RPi.
    │   │   ├── install_deps.bat        # -> Instala dependências no Windows (vcpkg, etc).
    │   │   └── download_models.py      # -> Baixa modelos base.
    │   ├── training/                   # Scripts do ATAD.
    │   │   ├── train.py
    │   │   └── requirements.txt
    │   └── deployment/
    │       ├── package_os.sh           # -> Cria um pacote de deploy para o TrackieOS.
    │       └── package_studio.bat      # -> Cria um pacote para o TrackieStudio.
    │
    ├── third_party/                    # -> Gerenciado via Git Submodules.
    │   ├── onnxruntime/
    │   └── llama.cpp/
    ├── .gitignore
    ├── CMakeLists.txt                  # -> Raiz do CMake: encontra pacotes e adiciona os subdiretórios dos módulos.
    └── Makefile
'''
---


## Sumário

1.  [Filosofia Arquitetural](#filosofia-arquitetural)
2.  [Estrutura de Diretórios](#estrutura-de-diretórios)
    *   [`/assets`](#assets)
    *   [`/scripts`](#scripts)
    *   [`/src` (O Coração do Projeto)](#src-o-coração-do-projeto)
        *   [`/src/core`](#srccore---a-mente)
        *   [`/src/hal`](#srchal---camada-de-abstração-de-hardware)
        *   [`/src/modules`](#srcmodules---o-corpo-funcional)
        *   [`/src/inference`](#srcinference---camada-de-inferência-de-modelos)
        *   [`/src/platform`](#srcplatform---otimizações-específicas)
        *   [`/src/shared`](#srcshared---código-compartilhado)
    *   [`/third_party`](#third_party)
    *   [`CMakeLists.txt`](#cmakeliststxt)
3.  [Conclusão](#conclusão)

---

## Filosofia Arquitetural

A arquitetura do TrackieLLM é baseada em três conceitos principais para garantir performance, modularidade e manutenibilidade:

1.  **A Mente (The Mind):** O orquestrador central (`/src/core`), que utiliza o modelo de linguagem Gemma para entender o contexto, tomar decisões e dar comandos. Ele não lida com processamento de baixo nível.
2.  **O Corpo (The Body):** Os módulos de processamento (`/src/modules`), que são especialistas em suas tarefas: visão computacional, processamento de áudio, etc. Eles são rápidos, eficientes e reativos.
3.  **O Sistema Nervoso (The Nervous System):** Um `EventBus` (`/src/core/event_bus.h`) que permite a comunicação assíncrona e desacoplada entre a "Mente" e o "Corpo". Isso previne gargalos e permite que cada componente opere de forma independente.

---

## Estrutura de Diretórios

### `/assets`

**Objetivo:** Armazenar todos os ativos não-código que a aplicação utiliza em tempo de execução.

*   **`/assets/models`**
    *   **Objetivo:** Centralizar todos os modelos de Machine Learning.
    *   **Arquivos:**
        *   `gemma-2b-it.gguf`: Modelo de linguagem do Google, no formato GGUF, otimizado para inferência em CPU via `llama.cpp`.
        *   `yolo_v8n.onnx`: Modelo de detecção de objetos (YOLOv8 Nano), no formato ONNX, escolhido pelo seu excelente balanço entre velocidade e acurácia em hardware limitado.
        *   `midas_v2_small.onnx`: Modelo de estimativa de profundidade (MiDaS), para medir distâncias a partir de uma imagem 2D.
        *   `mobilefacenet.onnx`: Modelo leve e eficiente para extrair *embeddings* (características matemáticas) de rostos, ideal para reconhecimento facial em tempo real.
*   **`/assets/configs`**
    *   **Objetivo:** Permitir a configuração da aplicação sem a necessidade de recompilar o código.
    *   **Arquivos:**
        *   `settings.json`: Configurações globais como resolução da câmera, caminhos para os modelos, limiares de confiança, etc.
        *   `dangerous_objects.json`: Uma lista de objetos que o módulo YOLO deve tratar com prioridade máxima, emitindo alertas imediatos (ex: "car", "bicycle", "staircase").
*   **`/assets/faces_db`**
    *   **Objetivo:** Manter uma base de dados persistente dos rostos conhecidos.
    *   **Arquivos:**
        *   `known_faces.dat`: Um arquivo binário que armazena os *embeddings* faciais calculados pelo MobileFaceNet e os nomes associados a cada rosto. Não armazena imagens, apenas as representações matemáticas.

### `/scripts`

**Objetivo:** Fornecer scripts para facilitar a compilação, execução e configuração do ambiente em diferentes sistemas operacionais.

*   **`/scripts/TrackieOS`** (para Raspberry Pi)
    *   **Objetivo:** Automatizar a operação no ambiente de produção (Linux ARM).
    *   **Arquivos:**
        *   `start.sh`: Script principal que define variáveis de ambiente (ex: para bibliotecas de baixo nível) e executa o binário principal do TrackieLLM com os parâmetros corretos.
        *   `setup_dependencies.sh`: Instala todas as dependências necessárias no Raspbian OS Lite (ex: `libv4l-dev`, `libasound2-dev`, `cmake`, etc.).
        *   `profile.sh`: Executa a aplicação sob ferramentas de profiling como `perf` ou `gprof` para identificar gargalos de performance.
*   **`/scripts/TrackieStudio`** (para Windows)
    *   **Objetivo:** Facilitar o desenvolvimento e depuração no Windows.
    *   **Arquivos:**
        *   `start.bat`: Script que configura o `PATH` para as DLLs necessárias (ex: ONNX Runtime) e inicia a aplicação.
        *   `setup_dependencies.bat`: Guia para instalação de dependências via `vcpkg` ou Chocolatey.

---

## `/src` (O Coração do Projeto)

### `/src/core` - A Mente

**Objetivo:** Orquestrar a lógica de alto nível da aplicação.

*   **`main.cpp`**
    *   **Objetivo:** Ponto de entrada da aplicação.
    *   **Funcionalidade:** Responsável pela inicialização e desligamento ordenado de todos os sistemas:
        1.  Carrega as configurações (`ConfigManager`).
        2.  Inicia o barramento de eventos (`EventBus`).
        3.  Inicializa a Camada de Abstração de Hardware (`HAL`).
        4.  Cria e inicializa todos os `Modules`.
        5.  Inicia o `Orchestrator`.
        6.  Entra no loop principal da aplicação e aguarda o sinal de término.
*   **`orchestrator.h/cpp`**
    *   **Objetivo:** O "cérebro" do TrackieLLM.
    *   **Funcionalidade:**
        *   Assina eventos de alto nível do `EventBus` (ex: `ObjectDetectedEvent`, `UserQuestionEvent`).
        *   Mantém o estado do contexto atual (ex: "andando na rua", "dentro de casa").
        *   Constrói *prompts* para o Gemma com base nos eventos recebidos e no contexto.
        *   Envia os *prompts* para o `LlamaRunner` e recebe a resposta em texto.
        *   Interpreta a resposta do Gemma e a traduz em comandos (ex: "tocar som de alerta", "descrever o ambiente").
        *   Publica esses comandos como novos eventos no `EventBus` (ex: `SpeakCommand`, `PlaySoundCommand`).
*   **`event_bus.h/cpp`**
    *   **Objetivo:** O "sistema nervoso" que desacopla os componentes.
    *   **Funcionalidade:** Implementa um padrão de design *Publish-Subscribe*. Módulos podem publicar eventos (ex: `vision_manager` publica `FrameProcessedEvent`) e outros módulos podem se inscrever para receber notificações sobre esses eventos.
    *   **Detalhes Técnicos:** Deve ser *thread-safe*, pois múltiplos módulos estarão publicando e consumindo eventos de threads diferentes. Utiliza mutexes ou locks para garantir a integridade da fila de eventos.
*   **`config_manager.h/cpp`**
    *   **Objetivo:** Carregar e fornecer acesso seguro às configurações do `settings.json`.
    *   **Funcionalidade:** Lê o arquivo JSON na inicialização e armazena os valores em uma estrutura de dados de fácil acesso. Fornece métodos como `getString("camera.device")` ou `getInt("yolo.input_width")`.

### `/src/hal` - Camada de Abstração de Hardware

**Objetivo:** Isolar o código que interage diretamente com o hardware, permitindo que o resto da aplicação seja agnóstico à plataforma. Utiliza um padrão de interface comum com implementações específicas por SO.

*   **`camera/camera_interface.h`**: Define a interface abstrata para uma câmera (`virtual void open()`, `virtual Frame grab_frame()`).
    *   `camera_linux.cpp`: Implementação para Linux usando a API V4L2.
    *   `camera_windows.cpp`: Implementação para Windows usando a API Media Foundation ou DirectShow.
*   **`microphone/mic_interface.h`**: Define a interface para o microfone.
    *   `mic_linux.cpp`: Implementação usando ALSA ou PulseAudio.
    *   `mic_windows.cpp`: Implementação usando WASAPI.
*   **`speaker/speaker_interface.h`**: Define a interface para o áudio de saída.
    *   `speaker_linux.cpp`: Implementação usando ALSA ou PulseAudio.
    *   `speaker_windows.cpp`: Implementação usando WASAPI.

### `/src/modules` - O Corpo Funcional

**Objetivo:** Encapsular as principais funcionalidades do sistema. Cada módulo roda em sua própria thread para não bloquear o resto da aplicação.

*   **`vision/`**: Responsável por todo o processamento de visão.
    *   `vision_manager.h/cpp`: Gerencia o pipeline de visão. Em sua thread, captura frames da câmera (`HAL`), e os envia para os submódulos de processamento.
    *   `yolo_detector.h/cpp`: Recebe um frame, executa a inferência com o `OnnxRunner`, e publica um `ObjectDetectedEvent` no `EventBus` para cada objeto encontrado com seus Bounding Boxes.
    *   `midas_estimator.h/cpp`: Se solicitado, recebe um frame, executa a inferência de profundidade e publica um `DepthMapEvent`.
    *   `face_recognizer.h/cpp`: Detecta rostos no frame. Para cada rosto, usa o `OnnxRunner` para extrair o *embedding* e o compara com a base de dados em `face_db`. Publica um `FaceRecognizedEvent`.
*   **`audio/`**: Responsável pelo processamento de áudio.
    *   `audio_manager.h/cpp`: Gerencia o microfone e o alto-falante.
    *   `speech_to_text.h/cpp`: Utiliza um modelo leve (ex: `Whisper.cpp`) para transcrever a fala do usuário em texto e publicar um `UserQuestionEvent`.
    *   `text_to_speech.h/cpp`: Assina eventos de comando de fala (`SpeakCommand`) e usa uma engine como `eSpeak-NG` ou Piper para sintetizar a voz e reproduzi-la no alto-falante.
*   **`database/`**:
    *   `face_db.h/cpp`: Gerencia a lógica de carregar, salvar e buscar *embeddings* no arquivo `known_faces.dat`. Fornece métodos para adicionar um novo rosto e encontrar o nome do rosto mais próximo a um dado *embedding*.

### `/src/inference` - Camada de Inferência de Modelos

**Objetivo:** Abstrair a complexidade dos runtimes de Machine Learning.

*   **`onnx_runner.h/cpp`**:
    *   **Objetivo:** Um wrapper em torno da API C++ do ONNX Runtime.
    *   **Funcionalidade:** Carrega um modelo `.onnx`, prepara os tensores de entrada (pré-processamento da imagem), executa a inferência e processa os tensores de saída. Usado por todos os módulos de visão.
*   **`llama_runner.h/cpp`**:
    *   **Objetivo:** Um wrapper em torno do `llama.cpp`.
    *   **Funcionalidade:** Carrega o modelo `.gguf`, gerencia o contexto da conversa, recebe um *prompt* de texto e gera a resposta token por token. Usado exclusivamente pelo `Orchestrator`.

### `/src/platform` - Otimizações Específicas

**Objetivo:** Conter código altamente otimizado e específico para uma arquitetura ou que necessita de garantias de segurança extras.

*   **`arm_optimizations/`**:
    *   **Objetivo:** Acelerar operações críticas em CPUs ARM (Raspberry Pi) usando Assembly ou intrínsecos NEON.
    *   **Arquivos:**
        *   `image_preprocess.S`: Código em Assembly para conversão de espaço de cores (ex: YUV para RGB) ou normalização de pixels, operações que são executadas em cada frame.
        *   `fast_math.S`: Funções matemáticas otimizadas, como produto escalar, usadas em cálculos de similaridade de *embeddings*.
*   **`rust_utils/`**:
    *   **Objetivo:** Implementar componentes críticos de segurança em Rust.
    *   **Arquivos:**
        *   `lib.rs`: Define a interface pública da biblioteca Rust, que será exposta para o C++ via FFI (Foreign Function Interface).
        *   `config_parser.rs`: Um parser para os arquivos JSON de configuração. Usar Rust aqui previne vulnerabilidades de segurança comuns em parsers escritos em C/C++, como buffer overflows.
        *   `Cargo.toml`: Arquivo de manifesto do projeto Rust.

### `/src/shared` - Código Compartilhado

**Objetivo:** Conter código e definições que são utilizados por múltiplos módulos.

*   **`types.h`**:
    *   **Objetivo:** Um dos arquivos mais importantes. Define todas as estruturas de dados personalizadas que fluem pelo sistema.
    *   **Exemplos:** `struct Frame { ... }`, `struct BoundingBox { ... }`, `struct FaceEmbedding { ... }`, e as próprias estruturas de evento para o `EventBus` (`struct ObjectDetectedEvent { ... }`).
*   **`logger.h/cpp`**: Utilitário de logging para registrar mensagens de debug, info, warning e error de forma consistente em toda a aplicação.
*   **`utils.h/cpp`**: Funções utilitárias genéricas (ex: manipulação de tempo, conversões de tipo, etc.).

---

### `/third_party`

**Objetivo:** Gerenciar dependências de terceiros. A melhor prática é usar `git submodules` para fixar versões específicas de cada biblioteca, garantindo builds reproduzíveis.

*   **`/onnxruntime/`**: Submódulo do repositório do ONNX Runtime.
*   **`/llama.cpp/`**: Submódulo do repositório do `llama.cpp`.

---

### `CMakeLists.txt`

**Objetivo:** O arquivo principal do sistema de build (CMake).

*   **Funcionalidade:**
    *   Define o nome do projeto e as versões de C++ e CMake.
    *   Encontra e configura as dependências (ONNX Runtime, `llama.cpp`, etc.).
    *   Define o executável principal (`trackiellm`).
    *   Adiciona todos os arquivos `.cpp` e `.h` aos alvos corretos.
    *   **Compilação Condicional:** Usa `if(WIN32)` e `if(UNIX)` para incluir os arquivos de implementação corretos da camada `HAL` (`camera_windows.cpp` vs `camera_linux.cpp`).
    *   **Integração com Rust:** Inclui regras para compilar a crate Rust (`/src/platform/rust_utils`) em uma biblioteca estática e linká-la ao executável principal.
    *   **Integração com Assembly:** Define regras para compilar os arquivos `.S` com o assembler apropriado (ex: `as` no GCC).

---

## Conclusão

Esta arquitetura modular e em camadas foi projetada para maximizar a performance em hardware restrito, ao mesmo tempo que oferece flexibilidade e facilidade de manutenção. A clara separação entre a "Mente" (lógica de decisão), o "Corpo" (processamento de dados) e o "Sistema Nervoso" (comunicação) permite que equipes trabalhem em módulos diferentes de forma paralela e garante que o sistema como um todo seja robusto e escalável.
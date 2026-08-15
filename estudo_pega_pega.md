# 📚 Documentação de Estudo — `pega_pega_turtlesim`

> Material elaborado como se fosse de um desenvolvedor sênior para um aluno.  
> Este pacote é um jogo de pega-pega com tartarugas no simulador turtlesim do ROS 2.  
> Objetivo de estudo: entender nós, pub/sub, serviços, timers, parâmetros e controle de robôs.

---

## 1. O que é esse pacote?

É um **jogo automático e infinito** rodando sobre o simulador `turtlesim`:

- Uma tartaruga **Pegador** persegue ativamente uma tartaruga **Vítima**
- A **Vítima** se move aleatoriamente tentando sobreviver
- Quando o **Pegador** alcança a **Vítima** (distância < 0,8 unidades), ela morre e renasce em posição aleatória
- A cor do fundo muda aleatoriamente a cada captura
- O ciclo se repete infinitamente

---

## 2. Fundamentos de ROS 2 usados nesse projeto

Antes de entender o código, você precisa ter clara a arquitetura do ROS 2.

### 2.1 O que é um Nó (Node)?

Um **nó** é um processo independente que executa uma tarefa específica. Nós se comunicam entre si por tópicos e serviços. No ROS 2, você cria um nó herdando de `rclcpp::Node`.

```cpp
class MeuNo : public rclcpp::Node {
public:
  MeuNo() : Node("nome_do_no") {
    // inicialização aqui
  }
};

int main(int argc, char *argv[]) {
  rclcpp::init(argc, argv);          // inicializa o ROS 2
  rclcpp::spin(std::make_shared<MeuNo>());  // inicia o loop de eventos
  rclcpp::shutdown();                // encerra o ROS 2
  return 0;
}
```

> 📌 `rclcpp::spin()` é o **executor**: o loop que fica aguardando e processando mensagens, timers e respostas de serviços. Sem ele, nada funciona.

---

### 2.2 Tópicos — Publicar e Assinar (Pub/Sub)

Tópicos são canais de comunicação assíncrona entre nós. Um nó **publica** dados, outro **assina** e recebe.

```
[Nó A] → publica em /topico → [Nó B] assina /topico → recebe os dados
```

**Regra fundamental**: Callbacks de subscriber devem ser **rápidas e não bloqueantes**. O executor é single-thread e não consegue processar mais nada enquanto uma callback estiver em execução.

```cpp
// Criar um subscriber
subscriber_ = this->create_subscription<turtlesim::msg::Pose>(
    "/vitima/pose",    // nome do tópico
    10,                // tamanho do buffer (QoS)
    std::bind(&MeuNo::minha_callback, this, _1));  // função chamada ao receber

// Criar um publisher
publisher_ = this->create_publisher<geometry_msgs::msg::Twist>(
    "/pegador/cmd_vel", 10);

// Publicar uma mensagem
geometry_msgs::msg::Twist msg;
msg.linear.x = 1.0;
publisher_->publish(msg);
```

---

### 2.3 Serviços — Requisição e Resposta (Request/Response)

Serviços são comunicação **síncrona por design**: um nó faz uma requisição e espera a resposta. No ROS 2, isso é feito de forma assíncrona com `async_send_request` para não bloquear o executor.

```
[Cliente] → envia Request → [Servidor] processa → envia Response → [Cliente] recebe
```

```cpp
// Criar um cliente de serviço
client_ = this->create_client<turtlesim::srv::Kill>("/kill");

// Verificar disponibilidade sem bloquear
if (!client_->service_is_ready()) { return; }

// Montar e enviar a requisição de forma assíncrona
auto request = std::make_shared<turtlesim::srv::Kill::Request>();
request->name = "vitima";

client_->async_send_request(
    request,
    [this](rclcpp::Client<turtlesim::srv::Kill>::SharedFuture future) {
        // esse lambda é executado quando a resposta chegar
        RCLCPP_INFO(this->get_logger(), "Serviço executado!");
    });
```

> ❌ NUNCA use `wait_for_service(timeout)` dentro de callbacks — isso bloqueia o executor!  
> ✅ Use `service_is_ready()` que retorna instantaneamente sem bloquear.

---

### 2.4 Timers

Timers executam uma função periodicamente, de dentro do executor (portanto são seguros e não bloqueantes).

```cpp
// Timer que dispara a cada 50ms (20 Hz)
timer_ = this->create_wall_timer(
    50ms,
    std::bind(&MeuNo::minha_funcao, this));

// Timer que dispara UMA VEZ (padrão de inicialização)
init_timer_ = this->create_wall_timer(
    500ms,
    [this]() {
        init_timer_->cancel(); // cancela a si mesmo
        // código de inicialização aqui
    });
```

---

### 2.5 Parâmetros em Runtime

Cada nó ROS 2 expõe automaticamente um serviço `/nome_do_no/set_parameters` que permite alterar parâmetros enquanto o nó está rodando. O tipo é `rcl_interfaces/srv/SetParameters`.

```cpp
rcl_interfaces::msg::Parameter param;
param.name = "background_r";
param.value.type = rcl_interfaces::msg::ParameterType::PARAMETER_INTEGER;
param.value.integer_value = 200;

auto request = std::make_shared<rcl_interfaces::srv::SetParameters::Request>();
request->parameters = {param};

bg_client_->async_send_request(request, callback);
```

---

## 3. Estrutura do Pacote

```
pega_pega_turtlesim/
├── CMakeLists.txt           ← sistema de build (quais arquivos compilar, dependências)
├── package.xml              ← metadados e dependências do pacote
├── launch/
│   └── pega_pega.launch.xml ← arquivo que inicia todos os nós de uma vez
└── src/
    ├── game_manager.cpp     ← Nó: gerenciador do jogo
    ├── turtle_pegador.cpp   ← Nó: tartaruga perseguidora
    └── turtle_vitima.cpp    ← Nó: tartaruga vítima
```

---

## 4. Grafo de Comunicação

Este é o **mapa completo** de como os nós se comunicam:

```
┌─────────────────────────────────────────────────────────────┐
│                        turtlesim_node                        │
│  Publica: /pegador/pose, /vitima/pose                        │
│  Serve:   /spawn, /kill, /turtlesim/set_parameters           │
└─────────────────────────────────────────────────────────────┘
         │ /pegador/pose      │ /vitima/pose
         ▼                    ▼
┌─────────────────┐   ┌──────────────────────────────────────┐
│  turtle_pegador │   │           turtle_vitima               │
│                 │   │                                       │
│ Subscreve:      │   │ Subscreve:                            │
│  /pegador/pose  │   │  /pegador/pose   /vitima/pose         │
│  /vitima/pose   │   │  /respawn_vitima                      │
│                 │   │                                       │
│ Publica:        │   │ Publica:                              │
│  /pegador/      │   │  /vitima/cmd_vel  /respawn_vitima     │
│    cmd_vel ─────┼───┼──────────────────────────────────┐   │
└─────────────────┘   │ Chama serviço: /kill             │   │
                       └──────────────────────────────────┼───┘
                                 │ /respawn_vitima         │ /kill
                                 ▼                         ▼
                       ┌──────────────────┐        turtlesim_node
                       │  game_manager    │
                       │                 │
                       │ Subscreve:       │
                       │  /respawn_vitima │
                       │                 │
                       │ Chama serviços:  │
                       │  /spawn          │
                       │  /kill           │
                       │  /turtlesim/     │
                       │  set_parameters  │
                       └──────────────────┘
```

**Legenda:**
- `───►` tópico (pub/sub assíncrono)
- `serviço` (req/resp síncrono)

---

## 5. Nó 1 — `game_manager`

**Arquivo:** [game_manager.cpp](file:///home/filipe/Desktop/ros2/my_workspace/src/projetos/pega_pega_turtlesim/src/game_manager.cpp)

### Responsabilidade
É o **maestro do jogo**. Ele gerencia o ciclo de vida das tartarugas e o estado visual (cor do fundo). Outros nós nunca falam diretamente com o `turtlesim_node` para criar/destruir tartarugas — apenas o `game_manager` tem essa responsabilidade.

### O que faz na inicialização (`iniciar_jogo`)
1. Aguarda 500ms (via timer) para garantir que o `turtlesim_node` está pronto
2. Remove a tartaruga padrão `turtle1` (para começar com ambiente limpo)
3. Spawna o `pegador` no centro da arena `(5.5, 5.5)`
4. Spawna a primeira `vitima` em posição aleatória

### O que faz ao receber `/respawn_vitima` (`respawn_callback`)
1. Chama `mudar_cor_background()` para trocar a cor do fundo para RGB aleatório
2. Aguarda 500ms (para o `/kill` terminar)
3. Spawna uma nova `vitima` em posição aleatória

### Clientes de serviço utilizados

| Serviço | Tipo | Para quê |
|---------|------|----------|
| `/spawn` | `turtlesim/srv/Spawn` | Criar novas tartarugas |
| `/kill` | `turtlesim/srv/Kill` | Remover tartarugas |
| `/turtlesim/set_parameters` | `rcl_interfaces/srv/SetParameters` | Mudar cor do fundo |

### Padrão de inicialização assíncrona

```cpp
// Construtor: apenas registra o timer
GameManager() : Node("game_manager") {
    timer_iniciar_ = this->create_wall_timer(
        500ms, std::bind(&GameManager::iniciar_jogo, this));
}

// Timer dispara 500ms após o spin começar — seguro para chamar serviços
void iniciar_jogo() {
    timer_iniciar_->cancel(); // só executa uma vez
    spawn_client_->wait_for_service(3s); // aqui é seguro: estamos DENTRO do spin
    // ...
}
```

> 💡 Note: `wait_for_service` dentro de um timer é tecnicamente bloqueante por curto período (3s), mas é aceitável na inicialização única. A versão ideal usaria a versão assíncrona com polling.

---

## 6. Nó 2 — `turtle_pegador`

**Arquivo:** [turtle_pegador.cpp](file:///home/filipe/Desktop/ros2/my_workspace/src/projetos/pega_pega_turtlesim/src/turtle_pegador.cpp)

### Responsabilidade
Controlar o movimento da tartaruga **pegador** para que ela persiga continuamente a **vítima** usando controle proporcional de velocidade.

### Como funciona

**Passo 1:** Dois subscribers recebem as posições atuais das tartarugas a ~62 Hz:
```
/pegador/pose  →  pegador_pose_callback  →  salva pegador_pose_
/vitima/pose   →  vitima_pose_callback   →  salva vitima_pose_ + atualiza timestamp
```

**Passo 2:** Um timer a 20 Hz (50ms) executa o loop de controle:
```
control_loop() chamada 20x por segundo
  → verifica se tem poses válidas
  → verifica se a vítima está viva (pose recente < 1.5s)
  → calcula ângulo e distância até a vítima
  → publica comando de velocidade em /pegador/cmd_vel
```

### A Matemática do Controle Proporcional

```
         vítima
           *
           |  dy
           |
pegador *──────
          dx

Ângulo alvo = atan2(dy, dx)     ← direção em que o pegador deve apontar
Erro de ângulo = atan2(sin(alvo - atual), cos(alvo - atual))  ← erro normalizado

angular.z = 4.0 × erro_ângulo   ← gira proporcionalmente ao desvio
linear.x  = min(2.0, 1.2 × distância)  ← anda proporcionalmente à distância
```

**Por que `atan2(sin(e), cos(e))` em vez de simplesmente `e`?**  
Porque ângulos dão wrap-around: `270°` e `-90°` são o mesmo ângulo, mas `270 - (-90) = 360`, não `0`. O truque do `atan2(sin, cos)` normaliza qualquer diferença angular para `[-π, π]`.

### Detecção de vítima ausente

```cpp
// Se a vítima foi morta, /vitima/pose para de ser publicado
// O pegador detecta isso pelo tempo sem atualização
if ((this->now() - last_vitima_time_).seconds() > 1.5) {
    // Publica velocidade zero → para de se mover
    geometry_msgs::msg::Twist stop_msg; // todos campos iniciam em 0
    pub_cmd_vel_->publish(stop_msg);
    return;
}
```

---

## 7. Nó 3 — `turtle_vitima`

**Arquivo:** [turtle_vitima.cpp](file:///home/filipe/Desktop/ros2/my_workspace/src/projetos/pega_pega_turtlesim/src/turtle_vitima.cpp)

### Responsabilidade
O nó mais complexo do sistema. Tem **três responsabilidades** simultâneas:
1. Mover a vítima de forma autônoma e aleatória
2. Detectar quando o pegador a alcançou
3. Gerenciar o ciclo de vida (morte → aguarda respawn → volta a viver)

### Máquina de Estados Implícita

O nó implementa uma máquina de estados informal com as flags booleanas:

```
                 ┌─────────────────────┐
                 │   VIVA (normal)     │◄─────────────────────┐
                 │  vitima_viva_ = true│                      │
                 │  aguardando_  = false│                     │
                 └────────┬────────────┘                      │
                          │ distancia < 0.8                   │
                          ▼                                   │
                 ┌─────────────────────┐              /vitima/pose
                 │   CAPTURADA         │              (nova tartaruga)
                 │  vitima_viva_ = false│             chega
                 │  publica /respawn   │                      │
                 │  chama /kill        │                      │
                 └────────┬────────────┘                      │
                          │ /respawn_vitima                   │
                          ▼  recebido                         │
                 ┌─────────────────────┐                      │
                 │   AGUARDANDO        ├──────────────────────┘
                 │  aguardando_ = true │
                 │  descarta poses     │
                 │  antigas em buffer  │
                 └─────────────────────┘
```

### Movimento Autônomo

Dois timers trabalham juntos:

```cpp
// Timer 1: a cada 1.5s, sorteia nova direção (velocidade linear e angular aleatórias)
move_timer_ = this->create_wall_timer(1500ms, [this]() {
    cmd_atual_.linear.x  = random(1.0, 2.0);
    cmd_atual_.angular.z = random(-2.0, 2.0);
});

// Timer 2: a cada 50ms, publica a velocidade atual (com correção de parede)
control_timer_ = this->create_wall_timer(50ms, [this]() {
    // Se perto de uma borda → vira rapidamente para dentro da arena
    if (perto_da_parede) {
        cmd_atual_.linear.x = 0.8;
        cmd_atual_.angular.z = 2.5;
    }
    pub_cmd_vel_->publish(cmd_atual_);
});
```

### Por que dois timers para o movimento?

**Separação de frequências:**
- O timer de `1500ms` define a **estratégia** (para onde ir)
- O timer de `50ms` faz a **execução** (publica o comando atual continuamente)

Se publicássemos a cada 1.5s apenas, o turtlesim receberia comandos de velocidade muito espaçados e a vítima pararia entre um e outro. Publicando a 20 Hz, o movimento é suave e contínuo.

### Detecção de Captura e Problema do Race Condition

```cpp
// Detecta captura
void verificar_captura() {
    if (!vitima_viva_ || !vitima_pose_recebida_ || !pegador_pose_recebida_) return;

    float dx = posicao_pegador_.x - posicao_vitima_.x;
    float dy = posicao_pegador_.y - posicao_vitima_.y;

    if (dx*dx + dy*dy < 0.64f) {   // raio² = 0.8² = 0.64
        vitima_viva_ = false;
        pub_respawn_->publish(msg_vazio);  // avisa o game_manager
        matar("vitima");                   // chama /kill
    }
}
```

**O problema do race condition (sutil e importante):**

Ao matar a vítima e publicar `/respawn_vitima`, o `game_manager` leva ~500ms para spawnar a nova vítima. Durante esse tempo, o buffer do tópico `/vitima/pose` ainda contém mensagens antigas (da vítima morta). Se simplesmente resetarmos `vitima_viva_ = true` imediatamente, lemos uma pose antiga e detectamos uma "captura dupla".

**Solução com flag de estado diferida:**

```cpp
// Ao receber /respawn_vitima:
void callback_respawn() {
    aguardando_respawn_ = true;      // NÃO reativa a captura aqui
    vitima_pose_recebida_ = false;   // descarta poses antigas
}

// Ao receber /vitima/pose:
void callback_vitima(msg) {
    posicao_vitima_ = *msg;

    // Só reativa QUANDO a primeira pose nova chegar
    if (aguardando_respawn_) {
        aguardando_respawn_ = false;
        vitima_viva_ = true;  // ← reativação segura, nova tartaruga confirmada
    }
    verificar_captura();
}
```

---

## 8. O Launch File

**Arquivo:** [pega_pega.launch.xml](file:///home/filipe/Desktop/ros2/my_workspace/src/projetos/pega_pega_turtlesim/launch/pega_pega.launch.xml)

```xml
<launch>
    <node pkg="turtlesim"         exec="turtlesim_node" name="turtlesim"/>
    <node pkg="pega_pega_turtlesim" exec="game_manager"  name="game_manager"/>
    <node pkg="pega_pega_turtlesim" exec="turtle_pegador" name="turtle_pegador"/>
    <node pkg="pega_pega_turtlesim" exec="turtle_vitima"  name="turtle_vitima"/>
</launch>
```

O launch file inicia **4 nós** de uma vez. A **ordem importa logicamente**, mas não tecnicamente — o ROS 2 inicia todos em paralelo. Por isso o `game_manager` usa o timer de inicialização: para garantir que o `turtlesim_node` esteja pronto quando ele tentar chamar `/spawn`.

Para executar:
```bash
source /opt/ros/jazzy/setup.bash
source ~/Desktop/ros2/my_workspace/install/setup.bash
ros2 launch pega_pega_turtlesim pega_pega.launch.xml
```

---

## 9. O Build System — CMakeLists.txt e package.xml

### package.xml — o manifesto do pacote

```xml
<depend>rclcpp</depend>          <!-- biblioteca core do ROS 2 C++ -->
<depend>turtlesim</depend>       <!-- msgs e srvs do turtlesim -->
<depend>std_msgs</depend>        <!-- std_msgs/Empty -->
<depend>geometry_msgs</depend>   <!-- geometry_msgs/Twist -->
<depend>rcl_interfaces</depend>  <!-- SetParameters service -->
```

> Todo pacote que você usar nos seus `#include` deve estar declarado aqui.

### CMakeLists.txt — como buildar

```cmake
# 1. Encontrar os pacotes instalados no sistema
find_package(rclcpp REQUIRED)
find_package(turtlesim REQUIRED)
find_package(geometry_msgs REQUIRED)
find_package(rcl_interfaces REQUIRED)

# 2. Declarar executáveis (um por nó)
add_executable(turtle_pegador src/turtle_pegador.cpp)
add_executable(turtle_vitima  src/turtle_vitima.cpp)
add_executable(game_manager   src/game_manager.cpp)

# 3. Linkár as dependências a cada executável
ament_target_dependencies(turtle_pegador rclcpp turtlesim geometry_msgs)
ament_target_dependencies(game_manager   rclcpp turtlesim std_msgs rcl_interfaces)

# 4. Instalar para que o ROS 2 encontre os binários
install(TARGETS turtle_pegador turtle_vitima game_manager
    DESTINATION lib/${PROJECT_NAME})

# 5. Instalar o diretório de launch
install(DIRECTORY launch/ DESTINATION share/${PROJECT_NAME}/)
```

**Checklist ao adicionar uma dependência nova:**
1. ✅ `#include` o header no `.cpp`
2. ✅ `<depend>nome</depend>` no `package.xml`
3. ✅ `find_package(nome REQUIRED)` no `CMakeLists.txt`
4. ✅ `nome` em `ament_target_dependencies(meu_executavel ... nome)`

---

## 10. Linha do Tempo de uma Captura — Tudo junto

```
t=0ms    Launch file inicia os 4 nós em paralelo
t=500ms  game_manager spawna pegador e vitima
         turtlesim começa a publicar /pegador/pose e /vitima/pose
t=550ms  turtle_pegador e turtle_vitima recebem as primeiras poses
         turtle_pegador começa a publicar /pegador/cmd_vel → pegador se move
         turtle_vitima começa a publicar /vitima/cmd_vel  → vitima se move

... jogo roda ...

t=Xms    distância entre pegador e vitima < 0.8 unidades
         turtle_vitima detecta no verificar_captura():
           → seta vitima_viva_ = false
           → publica em /respawn_vitima
           → chama /kill("vitima")

t=X+1ms  game_manager recebe /respawn_vitima:
           → chama mudar_cor_background() → fundo muda de cor
           → inicia timer de 500ms para respawn

         turtle_vitima recebe /respawn_vitima (publicou ele mesmo):
           → seta aguardando_respawn_ = true
           → para de aceitar poses antigas

t=X+50ms turtlesim mata a vitima (responde ao /kill)
          /vitima/pose para de ser publicado
          turtle_pegador detecta silêncio > 1.5s → para de se mover

t=X+500ms game_manager timer dispara:
            → chama spawn_vitima_aleatoria()
            → nova vitima aparece em posição aleatória

t=X+501ms turtlesim começa a publicar /vitima/pose da nova vitima
          turtle_vitima recebe a primeira pose nova:
            → aguardando_respawn_ = false → vitima_viva_ = true
            → captura reativada!
          turtle_pegador recebe a pose nova:
            → last_vitima_time_ atualizado → volta a perseguir

... ciclo recomeça ...
```

---

## 11. Conceitos de ROS 2 — Resumo Visual

```
┌──────────────────────────────────────────────────────────────────┐
│                    MECANISMOS DE COMUNICAÇÃO                      │
├──────────────────┬───────────────────────────────────────────────┤
│  TÓPICO          │ Assíncrono. Pub não sabe se alguém assinou.   │
│  (pub/sub)       │ N publicadores → N assinantes.                │
│                  │ Ideal para: streams de dados (sensores, poses)│
├──────────────────┼───────────────────────────────────────────────┤
│  SERVIÇO         │ Síncrono por design. Um servidor, N clientes. │
│  (req/resp)      │ Cliente envia Request, aguarda Response.       │
│                  │ Ideal para: ações pontuais (spawn, kill)       │
├──────────────────┼───────────────────────────────────────────────┤
│  PARÂMETRO       │ Valor configurável de um nó.                  │
│  (set/get)       │ Alterável em runtime via set_parameters.      │
│                  │ Ideal para: configurações (cor, velocidade)    │
└──────────────────┴───────────────────────────────────────────────┘

┌──────────────────────────────────────────────────────────────────┐
│                    REGRAS DE OURO DO EXECUTOR                     │
├──────────────────────────────────────────────────────────────────┤
│ ❌ NUNCA faça sleep_for() dentro de callbacks                    │
│ ❌ NUNCA faça wait_for_service() com timeout em callbacks        │
│ ❌ NUNCA faça chamadas de serviço síncronas em callbacks          │
│ ✅ Use create_wall_timer() para agendar trabalho futuro           │
│ ✅ Use async_send_request() com lambda para serviços              │
│ ✅ Use service_is_ready() para verificar sem bloquear             │
└──────────────────────────────────────────────────────────────────┘
```

---

## 12. Exercícios Propostos

Para fixar os conceitos deste projeto, tente implementar as seguintes melhorias:

1. **Placar:** Criar um nó `score_keeper` que conta quantas vítimas foram capturadas e publica em `/score` a cada respawn.

2. **Velocidade progressiva:** Fazer o pegador ficar cada vez mais rápido a cada captura (aumentar `Kp_linear` a cada `/respawn_vitima` recebido).

3. **Múltiplas vítimas:** Modificar o `game_manager` para spawnar N vítimas ao mesmo tempo. Como você gerenciaria os nomes dinâmicos (`vitima_1`, `vitima_2`, etc.)?

4. **Vítima esperta:** Em vez de mover aleatoriamente, a vítima calcula a direção oposta ao pegador e foge. (Dica: inverter o ângulo calculado com `atan2`).

5. **Arquivo de configuração:** Mover o raio de captura (0.8), a velocidade máxima (2.0) e o número de vítimas para parâmetros ROS 2 declarados no nó, configuráveis pelo launch file.

---

*Pacote: `pega_pega_turtlesim` | ROS 2 Jazzy | C++17*

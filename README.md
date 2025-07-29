# 📈 Datalogger de Movimento com IMU (MPU6050)

Este projeto implementa um **datalogger de movimento portátil** utilizando um microcontrolador Raspberry Pi Pico W e um sensor IMU MPU6050. Os dados de aceleração e giroscópio são capturados continuamente e armazenados em um cartão MicroSD em formato CSV. O sistema oferece **feedback em tempo real** ao usuário por meio de um display OLED, LEDs RGB, buzzer, além de interação via botões e interface serial.

---

## 📚 Tabela de Conteúdos

- [Visão Geral](#visão-geral)
- [Funcionalidades](#funcionalidades)
- [Hardware Utilizado](#hardware-utilizado)
- [Interação com o Usuário](#interação-com-o-usuário)
  - [Display OLED](#display-oled)
  - [LED RGB](#led-rgb)
  - [Buzzer](#buzzer)
  - [Botões](#botões)
- [Comandos Seriais](#comandos-seriais)
- [Formato dos Dados CSV](#formato-dos-dados-csv)
- [Análise Externa (Python)](#análise-externa-python)
- [Desenvolvedora](#desenvolvedora)

---

## 🔍 Visão Geral

O objetivo principal deste projeto é criar um **dispositivo autônomo capaz de registrar dados de movimento**. Ele é ideal para aplicações como:

- Análise de vibração
- Registro de atividades físicas
- Experimentos em robótica

Os dados coletados podem ser analisados posteriormente em um computador, com gráficos gerados via Python.

---

## ⚙️ Funcionalidades

- **Captura de Dados IMU**: Leitura contínua de aceleração (X, Y, Z) e giroscópio (X, Y, Z) do MPU6050  
- **Armazenamento em MicroSD**: Gravação dos dados em arquivo `.csv`  
- **Interface com o Usuário**: Feedback visual/sonoro via OLED, LED RGB e buzzer  
- **Controle por Botões**: Início/parada de gravação, montagem/desmontagem do SD  
- **Comunicação Serial**: Comandos para controle via terminal serial  
- **Gerenciamento de Erros**: Indicações visuais e sonoras para falhas  

---

## 🔌 Hardware Utilizado

- **Microcontrolador**: Raspberry Pi Pico W  
- **Sensor IMU**: MPU6050  
- **Cartão de Memória**: MicroSD  
- **Display**: OLED SSD1306  
- **LEDs**: LED RGB  
- **Áudio**: Buzzer  
- **Entrada**: Botões táteis  

---

## 🧠 Interação com o Usuário

### 📺 Display OLED

- `"Inicializando..."`: Sistema iniciando  
- `"SD Nao Montado"`: SD ausente  
- `"Montando SD..."` ou `"Desmontando SD..."`  
- `"SD Montado! / Pronto p/ uso"`  
- `"GRAVANDO..."`: Dados sendo registrados  
- `"ERRO!"`: Falha no IMU ou cartão SD  

---

### 🌈 LED RGB

- **Amarelo**: Inicializando / montando SD  
- **Verde**: Pronto para gravação  
- **Vermelho + Azul piscando**: Gravando  
- **Azul piscando**: Acessando SD  
- **Roxo piscando**: Erro crítico  

---

### 🔊 Buzzer

- 1 beep curto: Início da gravação  
- 2 beeps curtos: Parada da gravação  
- Sequência de beeps: Eventos do sistema  

---

### 🔘 Botões

- **Botão A**: Iniciar / Parar gravação  
- **Botão B**: Montar / Desmontar o SD  

> Os botões utilizam interrupções e debounce por software.

---

## 🖥️ Comandos Seriais

Com um terminal serial conectado ao Pico W:

| Comando | Função                         |
|---------|--------------------------------|
| `s`     | Iniciar / Parar gravação       |
| `m`     | Montar o cartão SD             |
| `u`     | Desmontar o cartão SD          |
| `l`     | Listar arquivos no SD          |
| `h`     | Mostrar ajuda dos comandos     |

---

## 📄 Formato dos Dados CSV

Os arquivos `.csv` seguem este cabeçalho:
numero_amostra,accel_x,accel_y,accel_z,giro_x,giro_y,giro_z

yaml
Copiar
Editar

- `numero_amostra`: Contador de amostras  
- `accel_*`: Aceleração nos eixos X, Y, Z  
- `giro_*`: Giroscópio nos eixos X, Y, Z  

---

## 📊 Análise Externa (Python)

Um script Python (`analysis.py`) pode ser usado para:

- Ler os arquivos CSV gerados
- Plotar gráficos de aceleração e rotação
- O eixo X representa o tempo (baseado na ordem das amostras)

---

## 👩‍💻 Desenvolvedora

- **Anna Beatriz Silva Lima**


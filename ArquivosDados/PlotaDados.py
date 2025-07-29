import numpy as np
import matplotlib.pyplot as plt

# Lê o arquivo CSV (pulando o cabeçalho)
data = np.loadtxt("imu_data.csv", delimiter=",", skiprows=1)

# Extrai os dados por coluna
amostra = data[:, 0]  # eixo X comum a todos os gráficos

accel_x = data[:, 1]
accel_y = data[:, 2]
accel_z = data[:, 3]

gyro_x = data[:, 4]
gyro_y = data[:, 5]
gyro_z = data[:, 6]

# Cria o gráfico da aceleração
plt.figure(figsize=(10, 4))
plt.plot(amostra, accel_x, label="Accel X", color='r')
plt.plot(amostra, accel_y, label="Accel Y", color='g')
plt.plot(amostra, accel_z, label="Accel Z", color='b')
plt.title("Dados de Aceleração")
plt.xlabel("Amostra")
plt.ylabel("Aceleração (raw)")
plt.grid()
plt.legend()
plt.tight_layout()
plt.show()

# Cria o gráfico do giroscópio
plt.figure(figsize=(10, 4))
plt.plot(amostra, gyro_x, label="Gyro X", color='r')
plt.plot(amostra, gyro_y, label="Gyro Y", color='g')
plt.plot(amostra, gyro_z, label="Gyro Z", color='b')
plt.title("Dados do Giroscópio")
plt.xlabel("Amostra")
plt.ylabel("Velocidade Angular (raw)")
plt.grid()
plt.legend()
plt.tight_layout()
plt.show()

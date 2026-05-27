import matplotlib.pyplot as plt

# Data
pwm = [1074, 1166, 1259, 1351, 1444, 1537, 1629, 1722, 1814, 1907, 2000]

hpa = [
    0.00,
    2.18684,
    5.56207,
    7.41330,
    8.30771,
    9.98726,
    11.41118,
    12.58217,
    13.907001,
    16.33036,
    18.87403
]

cmh2o = [
    0.00,
    2.2299521492,
    5.6717282592,
    7.5594655233,
    8.4715093606,
    10.1841696528,
    11.6361643034,
    12.8302356974,
    14.1811967992,
    16.652397982,
    19.2461475735
]

# Create figure
plt.figure(figsize=(9,5))

# Plot cmH2O
plt.plot(pwm, cmh2o, marker='o', linewidth=2, label='Pressure (cmH2O)')

# Plot hPa
plt.plot(pwm, hpa, marker='s', linewidth=2, label='Pressure (hPa)')

# Labels and title
plt.xlabel("BLDC Motor PWM", fontsize=16)
plt.ylabel("Pressure", fontsize=16)
# plt.title("BLDC Motor PWM vs Air Pressure", fontsize=18)

# Grid and legend
plt.grid(True)
plt.legend()

# Show graph
plt.show()
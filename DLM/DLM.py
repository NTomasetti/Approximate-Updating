#!/home/nltom2/.virtualenvs/Python/bin/python3
import edward as ed
from edward.models import Normal
import numpy as np
import tensorflow as tf
import matplotlib.pyplot as plt

#Setting up parameters for simulated data
T = 50
xsdTrue = 1
bTrue = 2
arTrue = 0.5
ysdTrue = 1

# Simulating data
yData = np.empty(T)
xData = np.empty(T+1)
xData[0] = np.random.normal(loc=0, scale=xsdTrue/np.sqrt(1 - arTrue**2))
for t in range(T):
    xData[t+1] = np.random.normal(loc=arTrue*xData[t], scale=xsdTrue)
    yData[t] = np.random.normal(loc=xData[t+1] + bTrue, scale=ysdTrue)

# Prior distributions
b = Normal(mu=0.0, sigma=5.0)
ar = Normal(mu=0.0, sigma=0.5)
xlv = Normal(mu=0.0, sigma=1.0)
ylv = Normal(mu=0.0, sigma=1.0)
xv = tf.exp(xlv)
yv = tf.exp(ylv)
#x and y via lists
x = [0] * (T)
y = [0] * (T)
x0 = Normal(mu=0.0, sigma=tf.sqrt(xv))
for t in range(T):
    if(t == 0):
        x[t] = Normal(mu=ar*x0, sigma=tf.sqrt(xv))
    else:
        x[t] = Normal(mu=ar*x[t-1], sigma=tf.sqrt(xv))
    y[t] = Normal(mu=x[t]+b, sigma=tf.sqrt(yv))

# Set up approximation
qAr = Normal(mu = tf.Variable(tf.random_normal([])),
            sigma=tf.nn.softplus(tf.Variable(tf.random_normal([]))))
qB = Normal(mu = tf.Variable(tf.random_normal([])),
            sigma=tf.nn.softplus(tf.Variable(tf.random_normal([]))))
qX0 = Normal(mu = tf.Variable(tf.random_normal([])),
            sigma=tf.nn.softplus(tf.Variable(tf.random_normal([]))))
qXlv = Normal(mu = tf.Variable(tf.random_normal([])),
            sigma=tf.nn.softplus(tf.Variable(tf.random_normal([]))))
qYlv = Normal(mu = tf.Variable(tf.random_normal([])),
            sigma=tf.nn.softplus(tf.Variable(tf.random_normal([]))))
qX = [0] * (T)
for t in range(T):
    qX[t] = Normal(mu = tf.Variable(tf.random_normal([])),
            sigma=tf.nn.softplus(tf.Variable(tf.random_normal([]))))

# Inference
parameters = {
    ar: qAr,
    b: qB,
    x0: qX0,
    xlv: qXlv,
    ylv: qYlv
}
states = {x: qX for x, qX in zip(x, qX)}
parameters.update(states)
data = {yt: yData for yt, yData in zip(y, yData)}
inference = ed.KLqp(parameters, data)
inference.run()

samples = qAr.sample(100).eval()
plt.hist(samples, bins = 20)
plt.savefig("qAr.png")
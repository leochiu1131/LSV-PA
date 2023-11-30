import matplotlib.pyplot as plt
squares = [1, 4, 9, 16, 25]
# 调用函数subplots,在一张图片中绘制一个或多个图表；
# fig，表示整张图片
# ax，表示图片中的各个图表
fig, ax = plt.subplots()
# 调用plot()方法，尝试根据给定的数据以有意义的方式绘制图表
ax.plot(squares)
# 函数plt.show()，打开Matplotlib查看并显示绘制的图表
plt.show()
#%%
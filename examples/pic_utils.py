import matplotlib.pyplot as plt
import matplotlib
import math
import numpy as np

def plot_lines(x_list, y_list, description_list, x_axis_name, y_axis_name, title, outputpath, x_range = None, y_range = None):

    plt.title(title)
    plt.xlabel(x_axis_name)
    plt.ylabel(y_axis_name)

    if x_range is not None:
        plt.xlim(x_range[0], x_range[1])
    if y_range is not None:
        plt.ylim(y_range[0], y_range[1])
    
    for i in range(len(x_list)):
        x = x_list[i]
        y = y_list[i]
        description = description_list[i]
        plt.plot(x, y, label = description)
    
    plt.legend()
    plt.savefig(outputpath)
    plt.close()

def plot_cdf_pic(val_list, description_list, x_axis_name, y_axis_name, title, outputpath, x_range = None, y_range = None):

    list_x = list()
    list_y = list()

    for i in range(len(val_list)):
        val = sorted(val_list[i])
        cdf_y = list()
        cdf_x = list()
        cdf_cnt = 0.0
        delta = 1 / len(val)

        index = 0
        while(index < len(val)):
            tempval = val[index]
            cnt = 1
            while(index < len(val) - 1 and val[index + 1] == tempval):
                index += 1
                cnt += 1
            cdf_cnt += cnt * delta
            cdf_y.append(cdf_cnt)
            cdf_x.append(tempval)
            index += 1
        list_x.append(cdf_x)
        list_y.append(cdf_y)

    plot_lines(list_x, list_y, description_list, x_axis_name, y_axis_name, title, outputpath, x_range, y_range)

def plot_bars(x_list, y_list, description_list, x_axis_name, y_axis_name, title, outputpath, x_range = None, y_range = None):

    plt.title(title)
    plt.xlabel(x_axis_name)
    plt.ylabel(y_axis_name)

    if x_range is not None:
        plt.xlim(x_range[0], x_range[1])
    if y_range is not None:
        plt.ylim(y_range[0], y_range[1])
    
    width = 0.8 / len(x_list)

    plt.xticks([index + width for index in range(len(x_list))], x_list)

    for i in range(len(y_list)):
        y = y_list[i]
        description = description_list[i]
        plt.bar(x=[j + width * i for j in range(len(y_list[i]))], height=y, width=width, label=description)
    
    plt.legend()
    plt.savefig(outputpath)
    plt.close()

def plot_acc_bars(x_list, y_list, description_list, x_axis_name, y_axis_name, title, outputpath, x_range = None, y_range = None):

    plt.title(title)
    plt.xlabel(x_axis_name)
    plt.ylabel(y_axis_name)

    if x_range is not None:
        plt.xlim(x_range[0], x_range[1])
    if y_range is not None:
        plt.ylim(y_range[0], y_range[1])

    pre_y = [0 for i in range(len(x_list))]
    alpha_list = [0.0, 0.35, 0.35, 0.83]
    color_list = ['darkorange', 'dodgerblue', 'r', 'r']
    for i in range(len(y_list)):
        y = y_list[i]
        description = description_list[i]
        print(np.arange(len(x_list)), y, pre_y)
        plt.bar(np.arange(len(x_list)), y, color = color_list[i], width = 0.6, bottom=pre_y, alpha=1.0 - alpha_list[i], label=description)
        for j in range(len(pre_y)):
            pre_y[j] += y[j]

    plt.xticks(np.arange(len(x_list)), x_list)
    plt.legend()
    plt.savefig(outputpath)
    plt.close()

def plot_acc_bars_horizontally(x_list, y_list, description_list, x_axis_name, y_axis_name, title, outputpath, x_range = None, y_range = None):

    plt.title(title)
    plt.xlabel(x_axis_name)
    plt.ylabel(y_axis_name)

    if x_range is not None:
        plt.xlim(x_range[0], x_range[1])
    if y_range is not None:
        plt.ylim(y_range[0], y_range[1])

    pre_y = [0 for i in range(len(x_list))]
    alpha_list = [0.0, 0.5, 0.66, 0.83]
    color_list = ['orange', 'g', 'b', 'r']
    for i in range(len(y_list)):
        y = y_list[i]
        description = description_list[i]
        print(np.arange(len(x_list)), y, pre_y)
        plt.barh(np.arange(len(x_list)), y, color = color_list[i], alpha=1.0 - alpha_list[i], label=description, height=0.5, left=pre_y)
        # plt.bar(np.arange(len(x_list)), y, color = color_list[i], bottom=pre_y, alpha=1.0 - alpha_list[i], label=description)
        for j in range(len(pre_y)):
            pre_y[j] += y[j]

    plt.yticks(np.arange(len(x_list)), x_list)
    plt.legend()
    plt.savefig(outputpath)
    plt.close()

# plot_bars(["test1", "test2", "test3"], [[1,1,1],[2,2,2],[3,3,3]], ["tar1", "tar2", "tar3"], "OH", "Y", "test", "test2.jpg")
# plot_cdf_pic([[0,0,1,2],[0,1,1,2]], ["line1", "line2"], "val", "cdf", "cdf-val", "test.jpg")
# plot_acc_bars(["test1", "test2", "test3", "test4"], [[1,1,1,1], [1,1,1,1], [1,1,2,1]], ["playable", "useful", "total"], "test", "Y", "test", "test3.jpg")
import numpy as np
import matplotlib.pyplot as plt


plt.rcParams['font.sans-serif'] = ['Arial Unicode MS','SimHei','DejaVu Sans']
plt.rcParams['axes.unicode_minus'] = False

INDICATORS = ['revenue', 'net_profit', 'roe', 'revenue_growth',
'gross_margin', 'debt_ratio', 'current_ratio', 'asset_turnover']

INDICATORS_CN = ['营业总收入', '净利润', 'ROE', '营收增长率',
'销售毛利率', '资产负债率', '流动比率', '总资产周转率']

BENEFIT_COLS = [0, 1, 2, 3, 4, 6, 7]



def load_data(filepath):
    data = np.genfromtxt(filepath,delimiter=',',skip_header=1,usecols=range(2,10),dtype=(float),encoding='utf-8-sig')
    names = np.genfromtxt(filepath,delimiter=',',skip_header=1,usecols=1,dtype=str,encoding='utf-8-sig')
    return data,names #usecols保留指定的列

def task1_overview(data):#算统计量、打表格、验证
    means = data.mean(axis=0)#均值
    stds  = data.std(axis=0)#标准差
    maxs  = data.max(axis=0)
    mins  = data.min(axis=0)
    print(f"{'指标':<16} {'均值':<12} {'标准差':<12} {'最大值':<12} {'最小值':<12}")
    print("-"*70)
    for i in range(8):
        print(f"{INDICATORS_CN[i]:<16} {means[i]:<12.2f} {stds[i]:<12.2f} "
              f"{maxs[i]:<12.2f} {mins[i]:<12.2f}")

def normalize_zscore(data):
    normalized = (data - data.mean(axis = 0))/data.std(axis = 0)
    return normalized

def task2_verify(normalized):
    print(f"均值{normalized.mean(axis = 0)},标准差{normalized.std(axis = 0)}")


def plot_radar(normalized, names, idx_a, idx_b):
    vals_a = normalized[idx_a]
    vals_b = normalized[idx_b]
    angles = np.linspace(0,2*np.pi,8,endpoint=False)#生成八卦图
    vals_a = np.append(vals_a, vals_a[0])
    vals_b = np.append(vals_b, vals_b[0])
    angles = np.append(angles, angles[0])
    fig, ax = plt.subplots(subplot_kw=dict(projection='polar'), figsize=(8, 8))
    
    ax.plot(angles, vals_a, 'o-', color='#E24B4A', label=names[idx_a])#9个角度，9个值，原点标记，实线链接，红色，实例
    ax.fill(angles, vals_a, alpha=0.15, color='#E24B4A')#9个角度，9个值，透明的0.15
    ax.plot(angles, vals_b, 's-', color='#378ADD', label=names[idx_b])
    ax.fill(angles, vals_b, alpha=0.15, color='#378ADD')

    #标注8个轴
    ax.set_thetagrids(angles[:-1] * 180 / np.pi, INDICATORS_CN)

    ax.legend(loc='upper right', bbox_to_anchor=(1.3, 1.1))
    ax.set_title(f'{names[idx_a]} vs {names[idx_b]}')
    plt.tight_layout()
    plt.savefig('radar.png', dpi=150)
    #plt.show()
    print("雷达图已保存为 radar.png")

def plot_correlation_heatmap(data):
    corr = np.corrcoef(data.T)
    fig,ax = plt.subplots(figsize=(8,7))
    im = ax.imshow(corr,cmap='coolwarm',vmin=-1,vmax=1)#会返回颜色对应的数值
    ax.set_xticks(range(8))#打标签
    ax.set_xticklabels(INDICATORS_CN,rotation=45,ha='right')
    ax.set_yticks(range(8))
    ax.set_yticklabels(INDICATORS_CN)
    #text（x,y），对于x是从左向右画，对于y是从上向下画（7在y轴最下面），
    for i in range(8):
        for j in range(8):
            ax.text(j, i, f'{corr[i, j]:.2f}', ha='center', va='center',
                    color='white' if abs(corr[i, j]) > 0.5 else 'black',
                    fontsize=9)
    fig.colorbar(im, label='相关系数')
    ax.set_title('8项财务指标相关性热力图')
    plt.tight_layout()
    plt.savefig('heatmap.png', dpi=150)
    print("热力图已保存为 heatmap.png")
    #plt.show()

def weighted_score(normalized,weights):
    scores = np.dot(normalized,weights)#相乘后求和
    return scores 

def task5_rankings(normalized, names):
    w_growth  = np.array([0.10, 0.10, 0.20, 0.30, 0.10, -0.10, 0.05, 0.05])
    w_value   = np.array([0.15, 0.20, 0.20, 0.05, 0.15, -0.15, 0.05, 0.05])
    w_balance = np.ones(8) / 8
    w_balance[5] = -1 / 8   # 均衡型：每个1/8，但debt_ratio取负
    schemes = [("成长型",w_growth),('价值型', w_value),('均衡型', w_balance)]
    for name , w in schemes:
        scores = weighted_score(normalized,w)
        top10_idx = np.argsort(scores)[-1:-11:-1]
        print(f"\n=== {name}投资者 Top-10 ===")
        for rank, idx in enumerate(top10_idx):
            print(f"  {rank+1:>2}. {names[idx]:<12}  得分: {scores[idx]:.4f}")

def topsis(data,weights,benefit_cols):
    normalized = (data - data.mean(axis = 0)) / data.std(axis = 0)
    weighted = normalized*weights

    m = data.shape[1]
    ideal_best = np.zeros(m)
    ideal_worst = np.zeros(m)
    for i in range(m):
        if i in benefit_cols:
            ideal_best[i] = weighted[:,i].max()
            ideal_worst[i] = weighted[:,i].min()
        else:
            ideal_best[i] = weighted[:,i].min()
            ideal_worst[i] = weighted[:,i].max()
    
    d_best = np.linalg.norm(weighted - ideal_best,axis=1)#按行算距离
    d_worst = np.linalg.norm(weighted - ideal_worst,axis=1)
    closeness = d_worst/(d_best + d_worst)

    ranking = np.argsort(closeness)[::-1]#返回的是排好序的[索引]
    return ranking,closeness

def task6_rankings(data,names):
    weights = np.ones(8)/8
    ranking ,closeness = topsis(data,weights,BENEFIT_COLS)
    print(f"\n=== TOPSIS 综合排名 Top-15 ===")
    for rank, idx in enumerate(ranking[:15]):#取位置和值
        print(f"  {rank+1:>2}. {names[idx]:<12}  贴近度: {closeness[idx]:.4f}")
    return ranking, closeness

def task7_compare(normalized, data, names):
    w = np.ones(8)/8
    w[5] = -1 / 8
    w_scores = weighted_score(normalized, w)
    w_ranking = np.argsort(w_scores)[::-1]#第一种方法的排名

    t_weights = np.ones(8) / 8
    t_ranking, t_closeness = topsis(data, t_weights, BENEFIT_COLS)#第二种方法的排名

    w_pos = np.zeros(494,dtype=int)
    t_pos = np.zeros(494,dtype=int)
    for rank , idx in enumerate(w_ranking):#取出来的是公司的索引值
        w_pos[idx] = rank+1#这里存的是公司排名
    for rank ,idx in enumerate(t_ranking):
        t_pos[idx] = rank+1
    diff = w_pos - t_pos
    order = np.argsort(np.abs(diff))[::-1][:15]
    print("\n=== 加权排名 vs TOPSIS排名(差异最大15家)===")
    print(f"{'公司':<12}{'加权排名':>8}{'TOPSIS排名':>12}{'差异':>8}")
    for idx in order:
        print(f"{names[idx]:<12}{w_pos[idx]:>8}{t_pos[idx]:>12}{diff[idx]:>8}")
    flag = 0
    for idx in order:
        if 'B' in names[idx] and flag > 0:
            continue
        print(f"\n--- {names[idx]} 深入分析 ---")
        print(f"加权排名:{w_pos[idx]}  TOPSIS排名:{t_pos[idx]}  差异:{diff[idx]}")
        print("原始指标:")
        for j in range(8):
            print(f"  {INDICATORS_CN[j]}: {data[idx, j]:.2f}")
        flag += 1
        if flag >= 2:
            break
    

def main():
    print('=' * 60)
    print(' 上市公司基本面量化评估系统')
    print('=' * 60)
    # 任务 1：加载 + 统计
    data, names = load_data('companies.csv')
    task1_overview(data)
    # 任务 2：标准化
    normalized = normalize_zscore(data)
    task2_verify(normalized)
    # 任务 3：雷达图（在公司名列表中查找你选的两家公司）
    idx_a = list(names).index('宁德时代')
    idx_b = list(names).index('比亚迪')
    plot_radar(normalized, names, idx_a, idx_b)
    # 任务 4：热力图
    plot_correlation_heatmap(data)
    # 任务 5：加权评分
    task5_rankings(normalized, names)
    # 任务 6：TOPSIS
    ranking, closeness = task6_rankings(data, names)
    # 任务 7：对比
    task7_compare(normalized, data, names)

    print('全部任务完成')

if __name__ == '__main__':
    main()
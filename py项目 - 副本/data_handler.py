#数据持久化
import csv
import os
from order import Order
import random
PRODUCTS = ["服务器", "防火墙", "交换机", "负载均衡器", "存储设备", "UPS电源", "GPU计算卡", "企业专线", "路由器", "云存储"]
SALESPERSONS = ["哪吒", "奥特曼", "悟空", "葫芦娃", "柯南", "鸣人", "路飞", "皮卡丘", "多啦A梦", "樱桃小丸子"]
def generate_sample_data_1():
    orders = [
        Order("ORD-001", "2025-01-05", "服务器", 5, 88000, "哪吒"),
        Order("ORD-002", "2025-01-12", "防火墙", 3, 45000, "悟空"),
        Order("ORD-003", "2025-01-20", "交换机", 10, 12000, "鸣人"),
        Order("ORD-004", "2025-02-03", "负载均衡器", 2, 68000, "哪吒"),
        Order("ORD-005", "2025-02-14", "存储设备", 4, 35000, "奥特曼"),
        Order("ORD-006", "2025-02-22", "UPS电源", 6, 8500, "皮卡丘"),
        Order("ORD-007", "2025-03-01", "GPU计算卡", 8, 120000, "路飞"),
        Order("ORD-008", "2025-03-10", "企业专线", 1, 50000, "柯南"),
        Order("ORD-009", "2025-03-18", "路由器", 5, 15000, "鸣人"),
        Order("ORD-010", "2025-03-25", "云存储", 3, 28000, "樱桃小丸子"),
        Order("ORD-011", "2025-04-02", "服务器", 2, 92000, "悟空"),
        Order("ORD-012", "2025-04-09", "防火墙", 4, 48000, "多啦A梦"),
        Order("ORD-013", "2025-04-16", "交换机", 8, 13500, "葫芦娃"),
        Order("ORD-014", "2025-04-23", "存储设备", 5, 32000, "哪吒"),
        Order("ORD-015", "2025-05-05", "GPU计算卡", 3, 115000, "路飞"),
        Order("ORD-016", "2025-05-12", "UPS电源", 10, 7800, "皮卡丘"),
        Order("ORD-017", "2025-05-20", "企业专线", 2, 55000, "柯南"),
        Order("ORD-018", "2025-05-28", "路由器", 6, 16000, "奥特曼"),
        Order("ORD-019", "2025-06-05", "云存储", 4, 26000, "樱桃小丸子"),
        Order("ORD-020", "2025-06-15", "服务器", 3, 90000, "鸣人"),
    ]
    return orders
#顺序 order_id,date,product,quantity,unit_price,salesperson
def save_to_csv(orders,filepath):
    #os.makedirs("data",exist_ok=True)#没文件的时候可以自动创文件
    with open(filepath,"w",newline="",encoding="utf-8") as f:
        writer = csv.DictWriter(f,fieldnames=["order_id","date", "product", "quantity", "unit_price","salesperson"])
        writer.writeheader()
        for order in orders:
            writer.writerow(order.to_dict())

def load_from_csv(filepath):
    orders = []
    try:
        with open(filepath,"r",newline="",encoding="utf-8") as f:

            reader = csv.DictReader(f)
            for row in reader:
                try:
                    order = Order(row["order_id"],row["date"],row["product"],int(row["quantity"]),float(row["unit_price"]),row["salesperson"])
                    orders.append(order)
                except (ValueError , KeyError):
                    print(f"某行字段数不匹配、数值字段格式错误")
    except FileNotFoundError:
        print(f"再找不到文件")
    return orders

def generate_sample_data():
    orders = []
    # for i in range(1,21):
    #     id = f"ORD-{i:03d}"
    #     data = f"2025-{random.randint(1,12):02d}-{random.randint(1,28):02d}"
    #     product = random.choice(PRODUCTS)
    #     quantity = random.randint(1,100)
    #     unit_price = random.randint(1000,50000)
    #     salesperson = random.choice(SALESPERSONS)
    #     order = Order(id,data,product,quantity,unit_price,salesperson)
    #     orders.append(order)
    orders = generate_sample_data_1()
    return orders

#===================================================================================================================================
# o1 = Order("ORD-001", "2025-01-15", "服务器", 2, 50000, "哪吒")
# o2 = Order("ORD-001", "2025-01-21", "服务器", 2, 50000, "哪吒")
# save_to_csv([o1,o2],"data111.csv")
# result = load_from_csv("data111.csv")
# for i in result:
#     print(i)
from order import Order
PRODUCTS = ["服务器", "防火墙", "交换机", "负载均衡器", "存储设备", "UPS电源", "GPU计算卡", "企业专线", "路由器", "云存储"]
SALESPERSONS = ["哪吒", "奥特曼", "悟空", "葫芦娃", "柯南", "鸣人", "路飞", "皮卡丘", "多啦A梦", "樱桃小丸子"]
class OrderManager:
    def __init__(self):
        self.orders = []
    def add_order(self,order):
        self.orders.append(order)
    def remove_order(self, order_id):
        for order in self.orders:
            if order.order_id == order_id:
                self.orders.remove(order)
                return
        raise ValueError(f"找不到{order_id}")
    def find_by_id(self,order_id):
        for order in self.orders:
            if order.order_id == order_id:
                return order
        return None
    def get_all_orders(self):
        return self.orders
    def set_orders(self,list,replace):
        if replace == True:
            self.orders = list
        else:
            self.orders+=list
    def search_orders(self,keyword):
        result = []
        keyword = keyword.lower()
        for order in self.orders:
            if keyword in  order.product.lower() or keyword in order.salesperson.lower():
                result.append(order)
        return result
    def total_revenue(self):
        total = 0
        for order in self.orders:
            total += order.total_amount()
        return total

    def monthly_revenue(self):
        result = {}
        for order in self.orders:
            month = order.date[:7]
            if month not in result:
                result[month] = 0
            result[month] += order.total_amount()
        return result

        
    def top_products(self,n = 4):
        a = {}
        for order in self.orders:
            if order.product not in a:
                a[order.product] = 0
            a[order.product] += order.total_amount()
        sort_list = sorted(a.items(),key = lambda x :x[1],reverse=True)
        return sort_list[:n]
    def top_salespersons(self,n = 3):
        a = {}
        for order in self.orders:
            if order.salesperson not in a:
                a[order.salesperson] = 0
            a[order.salesperson] += order.total_amount()
        sort_list = sorted(a.items(),key = lambda x :x[1],reverse=True)
        return sort_list[:n]
    def average_order_value(self):
        if len(self.orders) ==0:
            return 0
        else:
            return self.total_revenue() / len(self.orders)
    def summary_report(self):
        #先整理好需要的变量
        #第一个
        total_count = len(self.orders)
        total = self.total_revenue()
        average = self.average_order_value()
        #第二个
        month_product_tend = self.monthly_revenue()
        sorted_tend = sorted(month_product_tend.items())
        #第三个
        product_revenue_rank = self.top_products()
        #第四个
        sales_rank = self.top_salespersons()

        #开始拼接字符
        lis = []
        lis.append("")
        lis.append("==========销售统计报告==========")
        lis.append(f"订单数量:{total_count}")
        lis.append(f"总销售额：¥{total:,.2f}")
        lis.append(f"平均每单金额：¥{average:,.2f}")
        lis.append("")
        lis.append("【月销售趋势】")
        for month, amount in sorted_tend:
            lis.append(f"{month} ¥{amount:,.2f}")
        lis.append("")
        lis.append("【产品销售额排行】")
        i = 1 
        for product,money in product_revenue_rank:
            lis.append(f"{i}. {product}  ￥{money:,.2f}")
            i+=1
        lis.append("")
        lis.append("【销售人员业绩排行】")
        i = 1
        for sales,money_1 in sales_rank:
            lis.append(f"{i}. {sales}  ￥{money_1:,.2f}")
            i+=1
        lis.append("==================================")
        result = "\n".join(lis)
        return result

m = OrderManager()
m.add_order(Order("ORD-001", "2025-01-15", "服务器", 2, 50000, "哪吒"))
m.add_order(Order("ORD-002", "2025-01-20", "交换机", 5, 8000, "悟空"))
m.add_order(Order("ORD-003", "2025-02-01", "服务器", 1, 50000, "哪吒"))
m.add_order(Order("ORD-004", "2025-02-10", "路由器", 3, 12000, "柯南"))
m.add_order(Order("ORD-005", "2025-03-05", "存储设备", 2, 30000, "悟空"))
print(m.summary_report())
# a1 = Order("ORD-001", "2025-01-15", "服务器", 2, 50000, "奥特曼")
# test = OrderManager()
# test.add_order(a1)
# print(test.find_by_id("ORD-001"))
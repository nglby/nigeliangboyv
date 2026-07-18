PRODUCTS = ["服务器", "防火墙", "交换机", "负载均衡器", "存储设备", "UPS电源", "GPU计算卡", "企业专线", "路由器", "云存储"]
SALESPERSONS = ["哪吒", "奥特曼", "悟空", "葫芦娃", "柯南", "鸣人", "路飞", "皮卡丘", "多啦A梦", "樱桃小丸子"]
class Order:
    def __init__ (self,order_id,date,product,quantity,unit_price,salesperson):
        if product not in PRODUCTS:
            raise ValueError(f"产品不合法")
        if salesperson not in SALESPERSONS:
            raise ValueError(f"销售人不合法")
        if (quantity>100) or (quantity<1):
            raise ValueError(f"数量不合法")
        if (unit_price <= 0):
            raise ValueError(f"单价不能是负数")
        if (order_id[0] != "O" or order_id[1]!="R" or order_id[2]!="D" or order_id[3] != "-"):
            raise ValueError(f"ID错误")
        
        parts = date.split("-")
        if len(parts) != 3:
            raise ValueError(f"日期格式错误")
        month = int(parts[1])
        day = int(parts[2])
        if month < 1 or month > 12:
            raise ValueError(f"月份不合法")
        if day < 1 or day > 31:
            raise ValueError(f"日期不合法")
        self.order_id = order_id
        self.date = date
        self.product = product
        self.quantity = quantity
        self.unit_price = unit_price
        self.salesperson = salesperson
    def __str__(self):
        return f"{self.order_id} | {self.date} | {self.product} | {self.quantity}台 x ¥{self.unit_price} = ¥{self.total_amount()} | {self.salesperson}"
    def total_amount(self):
        return self.quantity * self.unit_price
    def to_dict(self):
        return {"order_id":self.order_id,"date":self.date,"product": self.product,"quantity": self.quantity,"unit_price": self.unit_price,"salesperson": self.salesperson}

# o1 = Order("ORD-001", "2025-01-15", "服务器", 2, 50000, "哪吒")
# print(o1.total_amount())

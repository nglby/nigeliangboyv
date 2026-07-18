from order import Order, PRODUCTS, SALESPERSONS
from manager import OrderManager
import data_handler
import csv
import os
import random
guest = OrderManager()
while True:
    print("===== 销售数据管理系统 =====")
    print("1. 查看所有订单")
    print("2. 搜索订单（按产品名或销售人员）")
    print("3. 添加单条订单")
    print("4. 删除订单")
    print("5. 查看销售统计报告")
    print("6. 保存数据到 CSV")
    print("7. 从 CSV 加载数据")
    print("8. 加载示例数据")
    print("9. 退出")
    choice = input("请选择：")
    
    if choice == "1":
        order = guest.get_all_orders()
        if len(order) == 0:
            print("没有订单")
        else:
            for i in order:
                print(i)
    elif choice == "2":
        keyword = input("输入订单（按产品名或销售人员）")
        order = guest.search_orders(keyword)
        for i in order:
            print(i)
    elif choice == "3":
        print("可选产品：", PRODUCTS)
        print("可选销售员：", SALESPERSONS)
        order_id = input("订单编号(ORD-开头）：")
        date = input("日期(YYYY-MM-DD):")
        product = input("产品名：")
        quantity = int(input("数量(1-100):"))
        unit_price = float(input("单价："))
        salesperson = input("销售人员：")
        order = Order(order_id, date, product, quantity, unit_price, salesperson)
        guest.add_order(order)
        print("添加成功")
    elif choice == "4":
        order_id = input("输入要删除的id号")
        guest.remove_order(order_id)
        print("删除成功")
    elif choice == "5":
        if len(guest.get_all_orders()) == 0:
            print("当前没有订单，无法生成报告")
        else:
            order = guest.summary_report()
            print(order)
    elif choice == "6":
        data_handler.save_to_csv(guest.get_all_orders,"data111.csv")
        print("保存完成")
    elif choice == "7":
        order = data_handler.load_from_csv("data111.csv")
        for i in order:
            print(i)
    elif choice == "8":
        order = data_handler.generate_sample_data()
        guest.set_orders(order,True)
        print("加载成功")
    elif choice == "9":
        print("再见！")
        break
    else:
        print("无效选项，请重新输入")
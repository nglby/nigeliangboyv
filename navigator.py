#     ros2路劲规划  107实验室
#     节点 
#     车每 0.1 秒该做什么动作?
#     仅107实验室使用
#     问题：里程计不是长久之策，请换上平面定位系统
#     意见交流邮箱 3134248423@qq.com
#                                            作者———你哥梁博宇  2026
import heapq #优先队列   引入的是 Python 自带的优先队列工具，A搜索时，手里有一堆待探索的格子，每次必须先探索看起来最有希望的那个，而不是按先后顺序。
import math 
import sys #读取命令行 ->你想输入的目的地坐标
import time

import rclpy#这是ROS2的python库
from rclpy.node import Node #节点的根基，没有节点，就用不了ros2
from geometry_msgs.msg import PoseStamped #引入的是格式，就是坐标，激光雷达和建图都是用的坐标，所以把消息类型都改成坐标格式
from nav_msgs.msg import Odometry, OccupancyGrid, Path#里程计，地图，路径
from sensor_msgs.msg import LaserScan #读取雷达的数据，距离根角度
import tf2_ros  #这是坐标变换查询库
from std_msgs.msg import String as StringMsg  #规定消息格式，用来给F4发指令

#这里都是参数区，可以根据不同的要求自己修改
GRID_STRIDE = 2         #slam的一格是5厘米，我们的2就是两个合为1格，就是一格是10厘米，这样主要是根据自己小车的大小来修改，如果用5厘米，算力就要提升4倍，得不偿失！
INFLATE_CELLS = 3      #障碍物向外膨胀3格，保证车远离障碍物
COST_CAP_CELLS = 8      #惩罚的范围，这里是8格，就是80厘米   ！！！膨胀指的是禁区，惩罚是做权重，膨胀的3格，是不允许进去的，惩罚的8格可以进去，但是需要考虑权重！！！
COST_WEIGHT = 0.0      #惩罚权重，0就是没有，我们只找最短的路
V_DRIVE = 400 #直行速度
V_SLOW = 230#慢速
V_BACK = -150#倒车速度
WZ_TURN = 45    #原地自转速度       
SM_WZ0 = 0         
WP_REACH = 0.15    #离中间路点15cm内表示已经到了，可以继续下一段路程
FINAL_TOL = 0.08     #最终停车的距离在目的地8cm以内，就表示到达了
ODOM_FIRST = False     #开关  
PASS_MIN = 0.80      #如果有走廊，一定要大于80厘米才会通过
GOAL_TOL = 0.35      #导航规划的时候，精度在35厘米以内算通过
ALIGN_TOL = 0.30       #陀螺仪角度容错，差不多是17度，不需要每次都精准转弯
FRONT_DANGER = 0.55    #前方55厘米内有障碍物，要停车，出现规划路径
FRONT_RESUME = 0.85    #危险解除后后退85厘米，重新找路
PROGRESS_M = 0.15      #如果8秒内，车移动小于15厘米，说明车卡住了
PROGRESS_T = 8.0       #这就是计时8秒


class Navigator(Node):   #这是自己写的对象，但是他继承了Node（Node是ROS2官方节点写的），拥有Node的属性，可以收发话题、查 TF、记日志

    def __init__(self, cli_goal):
        super().__init__("navigator") #注册自己 就叫“navigator”
        #这里用来存放数据
        # 回调函数收到新数据就更新这些变量，主循环 step() 随时来取
        self.map_msg = None  #slam地图的最新一帧
        self.map_seq = 0     # 地图更新计数器，用来记录更新了多少帧
        self.scan_seq = 0    #雷达更新计数器
        self.planned_map_seq = -1 #上次规划时用到的地图是第几帧
        self.planned_scan_seq = -1 ## 上次规划时用到的雷达是第几帧
        #！！！以上两个帧是用来保证地图的数据是本次雷达的数据
        self.last_plan_time = 0.0
        self.odom = None #里程计计数
        self.scan = None #雷达数据
        self.scan_time = 0.0 #用来检测雷达有没有超时

        #这里就到地图了，目的是轨迹画在地图坐标系里
        self.traj = Path() 
        self.traj.header.frame_id = "map"
        self.last_traj_point = None
        self.last_traj_pub = 0.0


        self.goal = None          # 目标点坐标
        self.recovery_clearance = False #是否处于膨胀恢复模式
        self.path = []            #规划出来的每个路点的列表
        self.state = "IDLE"       #当前状态
        self.keepalive = 0.0      #上次发V指令的时间，需要100ms发一次
        self.mark = (0.0, 0.0)    #记录卡住时车的坐标
        self.mark_t = 0.0
        self.esc_dir = 0          #脱困方向
        self.esc_t0 = 0.0         #脱困的开始时间
        self.log_i = 0            #已经被平滑处理后的转向量
        self.avoiding = False
        self.avoid_dir = 0

        #     ros2进群注册
        self.avoid_since = 0.0  #TF查询缓存  TF就是一个广播员，他把车现在的位置和角度发出来，注意！！！车的位置和角度，是雷达和里程计陀螺仪叠加解算出来的
        self.sm_wz = 0
        self.behind_dir = 0
        self.tf_buf = tf2_ros.Buffer()
        self.tf_listener = tf2_ros.TransformListener(self.tf_buf, self)
        self.pose = None       #车在地图帧的位姿，朝向查TF得到
        self.odom0 = None          # odom zero reference (launch point)
        self._last_q = None
        self.start_map_pose = None  #发车点坐标
        self.pending_relative_goal = None  #发送的目的地坐标
        self.goal_file_stamp = 0.0  #上次读取的时间

        #订阅四个数据相当于一个人加了四个群，可以从不同的群里获得不同的数据 下面四行就是四个话题的数据，自己是一个节点
        self.create_subscription(OccupancyGrid, "/map", self.cb_map, 10)
        self.create_subscription(Odometry, "/odom", self.cb_odom, 10)
        self.create_subscription(LaserScan, "/scan_front", self.cb_scan, 10)
        self.create_subscription(PoseStamped, "/goal_pose", self.cb_goal, 10)

        #RViz显示话题用TRANSIENT_LOCAL模式：晚打开RViz也能收到最后一条，不会直接卡死，以下是官方的固定代码
        from rclpy.qos import QoSProfile, DurabilityPolicy
        viz_qos = QoSProfile(depth=1, durability=DurabilityPolicy.TRANSIENT_LOCAL)
        self.path_pub = self.create_publisher(Path, "/planned_path", viz_qos)
        self.goal_pub = self.create_publisher(PoseStamped, "/current_goal", viz_qos)
        self.traj_pub = self.create_publisher(Path, "/traj", viz_qos)
        self.cmd_pub = self.create_publisher(StringMsg, "/car_cmd", 10)
        self.status_pub = self.create_publisher(StringMsg, "/nav_status", 10)

        self.spin_for(2.5)  #等待2.5秒，主要是等第一批数据到达
        if cli_goal:  #如果命令行输入了目的地坐标，那就注册相对坐标
            self.set_relative_goal(cli_goal[0], cli_goal[1])
        else:    #如果没有输入目的地坐标，就提醒你，输入目的地坐标
            self.get_logger().info(
                "ready - click '2D Goal Pose' in RViz, or run with x y args")

    #回调函数：数据到达时自动执行
    # ！！！注意这些函数不是自己调用的，是ROS2在数据到达时自动调用，类似于STM32的中断回调函数。
    # 它们只做一件事：把数据存进信箱变量，绝不做复杂计算，计算都放在主循环的step()中，！！中断要快进快出！！避免打乱时序和浪费时间

    def cb_map(self, m):
        self.map_msg = m  #收到新地图了，就存起来
        self.map_seq += 1 #自动加1，告诉主循环，图更新了
    #里程计消息里位置是(x,y)，朝向藏在四元数q里
    #四元数→偏航角：一个标准的数学变换，这里直接照抄就行了，所有我用过的陀螺仪都是这样解算的。
    def cb_odom(self, m):
        p = m.pose.pose.position
        q = m.pose.pose.orientation
        self._last_q = q
        self.odom = (p.x, p.y,
                     math.atan2(2 * (q.w * q.z + q.x * q.y),
                                1 - 2 * (q.y * q.y + q.z * q.z)))
    #这里是雷达
    def cb_scan(self, m):
        self.scan = m  #存下这一帧雷达数据
        self.scan_time = time.time()#记住这个时刻
        self.scan_seq += 1#雷达计数加一

    def cb_goal(self, m):
        self.set_goal(m.pose.position.x, m.pose.position.y)

    def set_goal(self, x, y):# 设定一个地图坐标的绝对目标
        self.recovery_clearance = False
        self.goal = (x, y)
        self.pending_relative_goal = None
        self.state = "PLAN"  #切换状态到 规划中
        self.get_logger().info("fixed map goal: (%.2f, %.2f)" % (x, y))

    def set_relative_goal(self, forward, left):# goto 3 -1 的含义，"从发车点起，向前 3 米，向左 -1 米（即右 1 米）"
        #为什么用相对目标？因为比赛发车点每次都一样，坐标是相对发车点的
        self.recovery_clearance = False
        self.pending_relative_goal = (forward, left)
        self._resolve_relative_goal()

    def _resolve_relative_goal(self):# 把"前3左-1"换算成地图坐标，本质是绕发车点旋转origin_yaw角度
            # (u,v) = (3,-1)，发车点朝向yaw0
            #地图x = x0 + u*cos(yaw0) - v*sin(yaw0)
            #地图y = y0 + u*sin(yaw0) + v*cos(yaw0)
            #这里可以仔细想想，如果我的坐标确定了，我目的地的坐标也确定了，那我们直接的角度是不是可以算出来
        if self.pending_relative_goal is None or self.start_map_pose is None:
            return
        u, v = self.pending_relative_goal
        x0, y0, yaw0 = self.start_map_pose
        c, sn = math.cos(yaw0), math.sin(yaw0)
        self.goal = (x0 + c*u - sn*v, y0 + sn*u + c*v)
        self.pending_relative_goal = None
        self.state = "PLAN"
        self.get_logger().info("fixed map goal: (%.2f, %.2f)" % self.goal)
        #以上都是角度的计算

    def poll_goal_file(self):#goto.sh 3 -1的实现，nav_cmd.py 把3000,-1000写进文件，
            #这里每0.1秒看一眼文件有没有被修改，所以是可以随时改变目的地的，不需要停车，重新烧录等等
        import os
        path = "/home/wyk0wjj0gr/nav_goal.txt"
        try:
            st = os.stat(path)
            if st.st_mtime <= self.goal_file_stamp:
                return  #文件没变，跳过
            a, b = open(path).read().strip().split(",", 1)
            self.goal_file_stamp = st.st_mtime
            self.set_relative_goal(int(a)/1000.0, int(b)/1000.0)  #文件里的是毫米整数，除以1000得到米为单位
        except (OSError, ValueError):   #如果错误，直接跳过
            pass

    def spin_for(self, sec):  #手动转空ROS2一段时间，用来初始化等待第一帧数据
        t0 = time.time()
        while time.time() - t0 < sec:
            rclpy.spin_once(self, timeout_sec=0.05)

    #这里是串口发送
    def send(self, c):#所有指令最终都走这里,往car_cmd频道发一串文字
            #stm32_odom守护进程收到后原样写进串口给固件
        self.cmd_pub.publish(StringMsg(data=c))#这里是调用官方的publish直接发送出去，StringMsy是打包一条消息

    def keep(self, c):#限速发送V指令最快100ms一条
            #为什么限速？发太快固件处理不过来,发太慢固件250ms收不到STM32会触发WATCHDOG直接停车
        if time.time() - self.keepalive > 0.10:
            self.send(c)
            self.keepalive = time.time()

    def stop(self):#发X就是停车
        self.send("X")
        self.keepalive = 0.0 #这里是清零，可以发其他消息，同时也不会在锁死阶段收不到其他指令

    # 雷达感知
    def front(self):
        #正前方±0.45弧度（±26°）扇区内，最近的障碍物距离
        s = self.scan
        if s is None:
            return 99.0
        i0 = int((-0.45 - s.angle_min) / s.angle_increment)#雷达返回的是一个普通的数组，只有弧度，这里是换算成数组下标，用int去取整，i0，i1表示左右26度
        i1 = int((0.45 - s.angle_min) / s.angle_increment)
        i0, i1 = max(i0, 0), min(i1, len(s.ranges) - 1)
        vals = [r for r in s.ranges[i0:i1 + 1]
                if s.range_min < r < s.range_max]
        return min(vals) if vals else 99.0  #只要有效距离，距离太近（车体本身）和太远（99米）都不要

    def side_free(self, sign):#侧方通道评分，25°/40°/55°三个角度的最小距离加权，分数越高，就是这个角度越空旷，这是为了在脱困的时候使用的逃脱手段
        vals = [self._sec(sign * math.radians(d), math.radians(10))
                for d in (25, 40, 55)]
        return min(vals) * 2 + sum(vals) * 0.3

    def _sec(self, center, half):
        s = self.scan    #通用扇区测距，以当前角度为中心±half弧度内最近障碍，看看哪边更空旷
        if s is None:
            return 99.0
        i0 = int((center - half - s.angle_min) / s.angle_increment)#雷达返回的是一个普通的数组，只有弧度，这里是换算成数组下标，用int去取整
        i1 = int((center + half - s.angle_min) / s.angle_increment)
        i0, i1 = max(i0, 0), min(i1, len(s.ranges) - 1)
        vals = [r for r in s.ranges[i0:i1 + 1]
                if s.range_min < r < s.range_max]
        return min(vals) if vals else 99.0    #只要有效距离，距离太近（车体本身）和太远（99米）都不要

    #   建格子，把世界变成A*的棋盘
    def build_grid(self, R=INFLATE_CELLS): #！！！！！！！！！！把SLAM地图+最新雷达→一张0/1棋盘（0=墙 1=空）+膨胀 + 代价场！！！！！！！！
        m = self.map_msg
        w, h = m.info.width, m.info.height #原始地图尺寸 ->一格5厘米
        stride = GRID_STRIDE               #这个宏在最初写了，是2，两格并一格
        sw, sh = w // stride, h // stride  #每一格就是10厘米
        #第1步：降采样，每 2x2 原始格合并成1个规划格 
        #只要4个小格里有一个被占（>50），整格算墙
        grid = bytearray(sw * sh)      #0=墙, 1=可走（默认可走）
        unknown = bytearray(sw * sh)   #未知区域，1 = 未知
        for r in range(sh):
            for c in range(sw):
                vals = [m.data[(r*stride+dr)*w+c*stride+dc]
                        for dr in (0, 1) for dc in (0, 1)]  #for循环采集四个格子的0，1数据
                i = r * sw + c
                grid[i] = 0 if any(v > 50 for v in vals) else 1  #如果有一个是1，四个全变1
                unknown[i] = 1 if any(v < 0 for v in vals) else 0
        #第2步：雷达直通
                #SLAM 发布地图有延迟（0.5Hz），雷达数据是 10Hz 的
                #把"当前这一帧"雷达的所有回波端点直接标成墙 —— 零延迟避障
                #具体装车中发现，彩色的地方也是障碍物，所以彩色也要被列到障碍物中
        if self.scan is not None and self.pose is not None:  #非空就是有数据
            px, py, yaw = self.pose   #有数据就拿过来
            scan = self.scan
            limit = min(float(scan.range_max), 6.0)   #只信任6米以内的雷达回波，超过6米误差大
            for k, distance in enumerate(scan.ranges):
                if not math.isfinite(distance) or not (scan.range_min < distance < limit):   #math库中的无穷大，如果6米内某个方向没障碍物，就认为这个方向的障碍物在无穷大的地方
                    continue
                angle = yaw + scan.angle_min + k * scan.angle_increment  #这里是已知数组下标，反推角度，因为雷达回波存到了数组里，只能通过数组下标读取，无法直接读到角度
                wx = px + distance * math.cos(angle)
                wy = py + distance * math.sin(angle)
                c = int((wx - m.info.origin.position.x) / (m.info.resolution * stride))
                r = int((wy - m.info.origin.position.y) / (m.info.resolution * stride))
                if 0 <= c < sw and 0 <= r < sh:
                    i = r * sw + c
                    grid[i] = 0    #置0，表示这个角度是墙
                    unknown[i] = 0 #既然是墙了，当然就不是未知区域了
        #第3步：膨胀。每个墙格向外扩R圈
        #目的，障碍物永远离车R圈，避免碰撞
        inf = bytearray(grid)  #复制一份地图，不能在原图上改，不然渲染了一次之后，下一次主循环运行到这里又会一直渲染，导致整个图铺满障碍物
        for r in range(sh):
            for c in range(sw):
                if grid[r*sw+c]:  #如果不是墙，直接跳过
                    continue
                for dr in range(-R, R+1):  #是墙就向外膨胀
                    for dc in range(-R, R+1):
                        rr, cc = r+dr, c+dc
                        if 0 <= rr < sh and 0 <= cc < sw:
                            inf[rr*sw+cc] = 0

        #第4步：代价场，算每个格子离最近墙有几格
        #用途：A*可以"宁可绕远也离墙远"
        #算法：从所有墙格同时向外"泛洪"，每走一步距离+1 。！！！流水算法——————今年福州大学同款！！！
        cap = COST_CAP_CELLS
        dist = bytearray([cap]) * (sw*sh)  #[cap]是一个字节大小，数值为8的数组，数组×（sw*sh）就是把数组重复1.4万次，得到的是全部填满8的棋盘，本质就是一个超大矩阵，这样默认每个格子都是8，也就是全都远离障碍物
        q = []
        for i, free in enumerate(inf):   #开始播种
            if not free:
                dist[i] = 0  #墙本身离自己就是0
                q.append(i)
        head = 0
        while head < len(q):     #开始往外泛洪
            i = q[head]; head += 1   #取队首，注意只用指针后移
            r, c = divmod(i, sw)     #取下标
            for dr, dc in ((1,0),(-1,0),(0,1),(0,-1)): 
                rr, cc = r+dr, c+dc
                if 0 <= rr < sh and 0 <= cc < sw: #取出上下左右四个邻居
                    j = rr*sw+cc
                    if dist[j] > dist[i]+1:  #如果到达边界，不再扩散
                        dist[j] = dist[i]+1
                        if dist[j] < cap:
                            q.append(j) #没有完成，继续往外泛洪
        return inf, dist, sw, sh, stride, unknown

    def cell_of(self, x, y):  #世界坐标 转成规划坐标
        m = self.map_msg
        ox, oy = m.info.origin.position.x, m.info.origin.position.y
        c = int((x - ox) / (m.info.resolution * GRID_STRIDE))
        r = int((y - oy) / (m.info.resolution * GRID_STRIDE))
        return c, r

    def world_of(self, c, r):  #规划坐标 转成世界坐标
        m = self.map_msg
        ox, oy = m.info.origin.position.x, m.info.origin.position.y
        return (ox + (c + 0.5) * m.info.resolution * GRID_STRIDE,
                oy + (r + 0.5) * m.info.resolution * GRID_STRIDE)
    #!!!!!!这里注意，路径规划器只认世界坐标，比如我要去第78列，第91行，但是小车走的是里程计只知道自己往前走了3米，往右走了1米，所以要来回转换！！！！！！
    # A*寻路，在地图上寻找最短的路径，我们用的是经典的f = g + h,g是从起点实际走了多远，h是起点根终点的直线距离
    def astar(self, grid, darr, unknown, sw, sh, start, goal): #如果目标点是障碍物，则无路可走
        if not (0 <= goal[0] < sw and 0 <= goal[1] < sh) or \
                not grid[goal[1] * sw + goal[0]]:
            return None
        def h(a, b):
            return math.hypot(a[0] - b[0], a[1] - b[1])
        openq = [(0.0, start)]   #优先列队，f值小的路优先被探索
        came = {}                #放个空字典，用来记录这个格子是从哪个格子走过来的
        g = {start: 0.0}         #算出每个格子的实际代价
        closed = set()           #记录已经被探索的格子
        while openq:
            _, cur = heapq.heappop(openq)   #取出来f最小的格子
            if cur == goal:                 #到家了
                path = [cur]                
                while cur in came:          #沿着came从目的地往出发点回溯
                    cur = came[cur]
                    path.append(cur)
                path.reverse()             #反转，从出发点往目的地
                return path
            if cur in closed:
                continue
            closed.add(cur)
            cc, rr = cur
            for dr, dc in ((1, 0), (-1, 0), (0, 1), (0, -1),    #探索自己的8个邻居 正4斜4
                           (1, 1), (1, -1), (-1, 1), (-1, -1)):
                nr, nc = rr + dr, cc + dc
                if not (0 <= nr < sh and 0 <= nc < sw):  #探出边界，跳过
                    continue
                nb = (nc, nr)
                if not grid[nr * sw + nc]:   #邻居是墙，跳过
                    continue
                if dr and dc:      #两个相邻的直格都必须是空的，防止从类似喇叭口形状钻进去
                    if not grid[rr * sw + nc] or not grid[nr * sw + cc]:
                        continue
                # soft cost: keep distance from obstacles (open-space paths)
                base = 1.414 if dr and dc else 1.0
                pen = (COST_CAP_CELLS - darr[nr * sw + nc]) * COST_WEIGHT
                #位置区域的代价为0，目标即使没扫到，也要推进，鼓励往靠近目标的未知区域探索
                explore_pen = 0.0
                ng = g[cur] + base + pen + explore_pen
                if ng < g.get(nb, 1e9): #找到了代价更低的路线，那就走这个低代价的路，注意！！！1e9是python中常表示无穷大，在没有任何路线的时候，代价为无穷大，有更低代价的路线的时候，直接赋值
                    g[nb] = ng
                    came[nb] = cur
                    heapq.heappush(openq, (ng + h(nb, goal), nb))
        return None

    def los(self, grid, sw, a, b):
        #bresenham 直线算法，a到b之间的每个格子都必须是空的
        (x0, y0), (x1, y1) = a, b
        dx, dy = abs(x1 - x0), abs(y1 - y0)
        sx = 1 if x0 < x1 else -1
        sy = 1 if y0 < y1 else -1
        err = dx - dy
        while True:
            if not grid[y0 * sw + x0]: #中间有墙，不通
                return False
            if (x0, y0) == (x1, y1):  #畅通
                return True
            e2 = 2 * err
            if e2 > -dy:
                err -= dy
                x0 += sx
            if e2 < dx:
                err += dx
                y0 += sy

    def simplify(self, grid, sw, path):  #贪心算法拉直，优先去视野内能扫到的最远的位置，如果情况很差的情况下，就是每走一个都要拐一个弯，那是一样的算力，但是如果是理想状态下，贪心算法可以一锤定音，一下直接扫出来直线路线，一步到位，不用走一格算一下
        out = [path[0]]
        i = 0
        while i < len(path) - 1:
            j = len(path) - 1
            while j > i + 1 and not self.los(grid, sw, path[i], path[j]):  #如果第一眼直接扫到了目的地，循环一次都不用进，如果没找到，就会进入循环一直找目的地
                j -= 1
            out.append(path[j])
            i = j
        return out
    #总调度：三档膨胀逐级尝试
    def plan(self):
        #完整规划流程，成功返回True并把路点存进self.path
        if self.map_msg is None or self.pose is None or self.goal is None:
            self.get_logger().info("waiting for map/odom...")
            return False

        # 对三种膨胀半径各试一次：3(30cm 正常)、2(20cm 挤)、1(10cm 极限)，车位或目标被高膨胀封死时，低膨胀可能还有路
        sc = self.cell_of(*self.pose[:2]) #车所在格
        gc = self.cell_of(*self.goal)     # 目的地所在格
        candidates = []
        radii = (2, 1) if self.recovery_clearance else (3, 2, 1)  #降档测试，优先选择安全的路，实在没招了才走冒险的路钻过去
        for R in radii:
            grid, darr, sw, sh, _, unknown = self.build_grid(R)
            if not (0 <= sc[0] < sw and 0 <= sc[1] < sh):  #车本身不在图内  直接报错
                self.get_logger().warn("robot outside map")
                return False
            if not grid[sc[1] * sw + sc[0]]:   #车自己站在墙里  不动，上报
                if R == 3:
                    self.recovery_clearance = True
                    self.get_logger().warn(
                        "robot entered the 30cm inflation band; using a local recovery plan")
                else:
                    self.get_logger().warn(
                        "robot is still inside obstacle cells at recovery clearance")
                continue
            if not (0 <= gc[0] < sw and 0 <= gc[1] < sh):   #目标在图外，上层探路，这里不规划
                self.get_logger().warn("goal outside current SLAM map")
                continue
            #三档全失败
            if not grid[gc[1] * sw + gc[0]]:
                continue
            raw = self.astar(grid, darr, unknown, sw, sh, sc, gc)
            if raw is None:
                continue
            cells = self.simplify(grid, sw, raw)  #规划成功，调用规划函数
            points = [self.world_of(c, r) for c, r in cells]
            if gc == sc:           
                route = [self.goal] if math.dist(self.pose[:2], self.goal) > FINAL_TOL else []    #特殊情况，发车点就是目的地，直接发已到达，车不动
            else:
                points[-1] = self.goal
                route = points
            length = sum(math.hypot(b[0] - a[0], b[1] - a[1])
                         for a, b in zip([self.pose[:2]] + route, route))
            candidates.append((R, route, length))

        if not candidates:        #无路可走
            self.get_logger().warn(
                "no path after normal and local recovery clearance - goal unreachable")
            return False

        #从候选里挑：默认用最宽的 30cm 路径；但如果低膨胀的路"几乎一样直"，且明显更短（省 ≥0.3m），就用低膨胀的直路穿过窄通道
        selected = candidates[0]
        direct = math.dist(self.pose[:2], self.goal)
        meaningful_gain = max(0.30, direct * 0.08) #至少省0.3米的路径，才值得去冒险
        near_direct_limit = direct * 1.15 + 0.20   #特殊情况  直接直线走就能到目的地
        for radius in (2, 1):
            candidate = next((c for c in candidates if c[0] == radius), None) #取出第一个满足条件的，即使挡位为2和1的，这两个挡位是需要冒险的
            if candidate is None:    #如果一个路线都没有，直接跳过
                continue
            near_direct = candidate[2] <= near_direct_limit
            if near_direct and selected[2] - candidate[2] >= meaningful_gain:   #降档了，路程要足够短，不然就会选择去绕行了，降档必须省下大路程才能走。
                previous = selected
                selected = candidate
                self.recovery_clearance = True
                self.get_logger().warn(
                    "using %d0cm clearance direct shortcut (%.1fm vs %.1fm detour)" %
                    (radius, candidate[2], previous[2]))

        R, self.path, path_len = selected
        if R < 3:
            self.get_logger().warn(
                "only %d0cm clearance path found - drive with care!" % R)
            #把选中的路径发到RViz显示
        pmsg = Path()
        pmsg.header.frame_id = "map"
        pmsg.header.stamp = self.get_clock().now().to_msg()
        for x, y in [self.pose[:2]] + self.path:
            ps = PoseStamped()
            ps.header.frame_id = "map"
            ps.pose.position.x = x
            ps.pose.position.y = y
            pmsg.poses.append(ps)
        self.path_pub.publish(pmsg)
        #目标箭头也发出去
        gmsg = PoseStamped()
        gmsg.header.frame_id = "map"
        gmsg.pose.position.x = self.goal[0]
        gmsg.pose.position.y = self.goal[1]
        gmsg.pose.orientation.w = 1.0
        self.goal_pub.publish(gmsg)
        self.get_logger().info("path(inf=%d0cm): %d waypoints, %.1f m" %
                               (R, len(self.path), path_len))
        self.state = "GO"         #状态切到行驶状态
        self.planned_map_seq = self.map_seq
        self.planned_scan_seq = self.scan_seq
        self.last_plan_time = time.time()
        self.status_pub.publish(StringMsg(data="MOVING"))
        self.mark = (self.pose[0], self.pose[1])
        self.mark_t = time.time()
        return True

    #卡住检测
    def _stalled(self):
        if self.pose is None:    #八秒内位移小于15厘米，就是卡住了，比如空转，撞墙
            return False
        moved = math.hypot(self.pose[0] - self.mark[0],
                           self.pose[1] - self.mark[1])
        if moved >= PROGRESS_M:   #有进展，重新计时
            self.mark = (self.pose[0], self.pose[1])
            self.mark_t = time.time()
            return False
        return time.time() - self.mark_t > PROGRESS_T

    def update_pose(self):
        #查车在地图帧的实时位姿，每0.1秒一次
        try:
            t = self.tf_buf.lookup_transform(
                "map", "base_link", rclpy.time.Time())
            q = t.transform.rotation
            yaw = math.atan2(2*(q.w*q.z+q.x*q.y),
                             1-2*(q.y*q.y+q.z*q.z))
            self.pose = (t.transform.translation.x,
                         t.transform.translation.y, yaw)
            if self.start_map_pose is None:  #第一次查到车的信息
                self.start_map_pose = self.pose
                self._resolve_relative_goal()
        except Exception:   #TF没拿到数据
            self.pose = None     #这里用try,except，没数据只会等数据，不会报错

    def publish_traj(self):
        #橙色实际轨迹：车每移动 5cm 记一个点，0.5 秒发一次
        if self.pose is None:
            return
        now = time.time()
        point = self.pose[:2]
        if self.last_traj_point is None or math.dist(point, self.last_traj_point) >= 0.05:
            ps = PoseStamped()
            ps.header.frame_id = "map"
            ps.header.stamp = self.get_clock().now().to_msg()
            ps.pose.position.x, ps.pose.position.y = point
            ps.pose.orientation.w = 1.0
            self.traj.poses.append(ps)
            self.last_traj_point = point
            if len(self.traj.poses) > 4000:
                self.traj.poses = self.traj.poses[-4000:]
        if now - self.last_traj_pub >= 0.5:
            self.traj.header.stamp = self.get_clock().now().to_msg()
            self.traj_pub.publish(self.traj)
            self.last_traj_pub = now
    #step 状态机：每 0.1 秒执行一次的主循环
    def step(self):
        self.update_pose()        #刷新车位
        self.publish_traj()       #画轨迹
        self.poll_goal_file()     #检查有没有新目标
        self._resolve_relative_goal()   # 有待换算的目标就换算
        if self.state == "IDLE":
            self.keep("X")        #空闲,持续发X,保持急停状态
            return
        if self.state == "PLAN":
            #规划限频：最快0.5秒一次（A*要算路径，不能把CPU跑满）
            if time.time() - self.last_plan_time < 0.5:
                return
            if not self.plan():
                self.stop()
                self.last_plan_time = time.time()
                self.status_pub.publish(StringMsg(data="NO_PATH"))
            return
        if self.state in ("GO", "AVOID"):       #雷达数据超过1秒没更新，立刻停车
            if self.scan is None or time.time() - self.scan_time > 1.0:
                self.keep("X")
                return
            #地图或雷达有新数据就得重新规划，这是核心，不能省
            if (self.map_seq != self.planned_map_seq or
                    self.scan_seq != self.planned_scan_seq) and \
                    time.time() - self.last_plan_time >= 0.5:
                self.state = "PLAN"
                return
            if self.pose is None:
                self.keep("X")   #定位消失，停车
                return 
            if not self.path:     #路点全部走完，到达目的地，进入空闲时刻，保持停车
                self.state = "IDLE"   
                self.status_pub.publish(StringMsg(data="ARRIVED"))
                return
            wp = self.path[0]       #当前要去的路点
            dist = math.hypot(wp[0] - self.pose[0], wp[1] - self.pose[1])
            is_final = (len(self.path) == 1)   #判断是不是最后一个路点
            tol = FINAL_TOL if is_final else WP_REACH     #对目的地是8厘米，对路点是15厘米，到达这个范围内就判为到达
            if dist < tol:
                self.path.pop(0)     #到达当前路点，弹出
                if not self.path:
                    self.stop()
                    self.get_logger().info(
                        "GOAL REACHED (err %.2fm)" % dist)
                    self.state = "IDLE"
                    self.status_pub.publish(StringMsg(data="ARRIVED"))
                return
            if self._stalled():
                self.get_logger().warn("stalled -> replan")
                self.stop()
                self.state = "PLAN"
                return

            #弧线转向的核心公式
            bearing = math.atan2(wp[1] - self.pose[1], wp[0] - self.pose[0])
            #herr=航向误差：路点方向与车头方向差多少弧度
            herr = (bearing - self.pose[2] + math.pi) % (2 * math.pi) - math.pi
            #(+π)%2π-π 是标准的"角度归一化到±π"，防止-350°和+10°被当成两回事
            # 正前方35cm内有东西且车头基本对准，停下重规划
            if abs(herr) < 0.35 and self.front() < FRONT_DANGER:
                self.stop()
                self.state = "PLAN"
                self.last_plan_time = 0.0
                return
            #打舵量=误差×220，限幅 ±120，用来转弯
            wz = max(-120, min(120, int(herr * 220)))
            self.sm_wz = int(0.35 * self.sm_wz + 0.65 * wz)  #用来平滑打舵量，防止猛冲
            v = V_DRIVE
            if abs(herr) > 0.8:    #误差大，减速过弯
                v = 140
            elif abs(herr) > 0.30:
                v = 280
            if is_final and dist < 0.6:   #快要到目的地了，得减速，防止冲过去
                v = min(v, 220)
            self.keep("V%d,0,%d" % (v, self.sm_wz))
        elif self.state == "FW_MOVE":   #前进脱困状态，3秒后回PLAN检查真实误差
            if time.time() - self.esc_t0 > 3.0:
                self.state = "PLAN"  
        elif self.state == "BACKUP":
            self.keep("V%d,0,0" % V_BACK)
            if time.time() - self.esc_t0 > 1.2:
                self.stop()
                self.state = "PLAN"

    def pick_direction(self, bearing):
        #在路点方向±90°范围内找最宽的可通过方，。每15°一个候选，每个候选用三根射线（±12°）测通道宽度，≥80cm算可通过，取离原方向最近的，全被堵就取最宽的那个。
        cands = []
        for deg in range(-90, 91, 15):
            off = math.radians(deg)
            # corridor width at this angle: three rays must all be clear
            rays = [self._sec(bearing + off + math.radians(dd),
                              math.radians(6))
                    for dd in (-12, 0, 12)]
            passage = min(min(rays), 1.6)          #上限1.6m，够宽就行了
            cands.append((off, passage))
        passable = [(o, p) for o, p in cands if p >= PASS_MIN]
        if passable:
            passable.sort(key=lambda t: abs(t[0]))
            return passable[0]
        cands.sort(key=lambda t: -t[1])    #全堵 --- 泰科打法，选最宽的方向直接硬闯，注意！这里已经是在绝境了，选择硬闯最宽的地方，12个方向内的最宽的方向是有距离的，如果是一点距离都没有的，直接停车，硬闯也是冒险的，无望的情况下，就会停车
        return cands[0]

    def shutdown(self):
        #程序退出前：先急停再失能电机（安全第一）
        self.send("X")
        self.send("E0")


def main():
    cli = []
    if len(sys.argv) >= 3:
        cli = [(float(sys.argv[1]), float(sys.argv[2]))]
    rclpy.init()
    nav = Navigator(cli)
    try:
        while rclpy.ok():
            rclpy.spin_once(nav, timeout_sec=0.1)
            nav.step()
    except KeyboardInterrupt:
        pass
    finally:
        nav.get_logger().info("shutdown: X + E0")
        nav.shutdown()
        nav.destroy_node()
        rclpy.shutdown()


if __name__ == "__main__":
    main()

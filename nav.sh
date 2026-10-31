#     ROS2   全车总启动
#     107实验室
#                    你哥梁博宇  2026 

# 不客气

#!/bin/bash

pgrep -f "rec2.sh" >/dev/null || (setsid nohup bash /home/wyk0wjj0gr/rec2.sh >/dev/null 2>&1 < /dev/null &) #检查录屏器， ||表示如果前面成立，就不运行后面，如果前面不成立，就运行后面，也就是如果录屏器正常，就继续，如果录屏器不正常，就静默拉起
echo "=========================================="  #这里可以理解成printf
echo " 发车检查：车是否已放在发车点（泡沫直角处）"
echo "           车头朝向 = 比赛发车朝向？"
read -p " 确认请输 y，否则回车取消: " ok #先打印后读取命令行输入，输入的信息赋给ok
[ "$ok" = "y" ] || { echo "已取消"; exit 1; }    #如果ok是y就跳过 ||后面的内容，如果不是y，就会打印取消，并且直接结束
echo "=========================================="  #打印
pkill -9 -f nav_direct.py 2>/dev/null    #pkill -9是强制杀掉进程，以下带 -f的是完整命令行匹配，所有相关的进程都会被杀，-x只会精准的杀单独匹配的进程，这一行是杀老版本是导航系统，我们已经不用了的
pkill -9 -f navigator.py 2>/dev/null      #这是杀现任的车的大脑，因为我们会改变逻辑，参数等等，所以每次都要杀掉旧的
rm -f /home/wyk0wjj0gr/nav_goal.txt   # no stale goals across sessions  #我们给车的目的地坐标是放在这个文件里的，如果不杀，车会直接读取上次的目的地去跑
pkill -9 -f nav_guard.py 2>/dev/null     #同nav_direct.py，是旧版的守卫，我们已经不用了
pkill -9 -f slam_toolbox 2>/dev/null      #杀掉旧地图，每次发车都会重新建图
pkill -9 -f lslidar_driver_node 2>/dev/null   #雷达串口走USB是独占的，旧实例不死新的打不开串口
pkill -9 -f scan_front_filter.py 2>/dev/null   #杀死旧的雷达过滤器
pkill -9 -f stm32_odom.py 2>/dev/null         #独占串口，不杀掉，新的进程想占串口都得繁忙
pkill -9 -f static_transform_publisher 2>/dev/null   #这是TF发布，如果不杀，会重复发布
pkill -9 -x rviz2 2>/dev/null                      #rviz2是建图软件，不能全杀，比如自己的配置文件
sleep 2           #睡两秒，保证杀干净

rm -f /dev/shm/fastrtps_* /dev/shm/iceoryx_* 2>/dev/null  # 清DDS共享内存。ROS2进程间传数据走/dev/shm里的共享内存段进程被kill -9强杀时来不及清理，尸体段留在那里，新进程启动时和残留段匹配错乱
sleep 1

PORT=$(cat /home/wyk0wjj0gr/car_port.txt 2>/dev/null)
if [ -z "$PORT" ] || [ ! -e "$PORT" ]; then            #自动查找车的串口
  PORT=$(ls /dev/ttyACM* /dev/ttyUSB* 2>/dev/null | head -1)
fi
echo "car serial: ${PORT:-NONE - plug the module in!}"
source /opt/ros/jazzy/setup.bash
source /home/wyk0wjj0gr/ros2_ws/install/setup.bash    #ROS2

export LIBGL_ALWAYS_SOFTWARE=1
echo "[1/4] sensors: lidar + scan filter + odom + static TF..."
setsid bash -c "nohup ros2 launch lslidar_driver lslidar_x10_launch.py >> /tmp/lidar.log 2>&1 < /dev/null &"   #打开四个运行的py脚本，&是不阻塞脚本，脚本可以继续往下，/dev/null是断开键盘，在开启过程中键盘不允许输入，setsid脱离终端会话，加入新会话组，和原终端彻底断绝关系
sleep 2
setsid bash -c "nohup python3 -u /home/wyk0wjj0gr/scripts/scan_front_filter.py >> /tmp/scanfilter.log 2>&1 < /dev/null &"
setsid bash -c "nohup python3 -u /home/wyk0wjj0gr/scripts/stm32_odom.py >> /tmp/sodom.log 2>&1 < /dev/null &"
setsid bash -c "nohup ros2 run tf2_ros static_transform_publisher --x 0 --y 0 --z 0.15 --frame-id base_link --child-frame-id laser >> /tmp/tf2.log 2>&1 < /dev/null &"
sleep 4

ros2 topic pub --once /car_cmd std_msgs/msg/String "{data: 'O'}" > /dev/null 2>&1  #ros2 topic pub是往话题里发一条消息，--once是只发一次，然后自动退出，/car_cmd是目标话题，std_msgs/msg/String是消息格式，"{data: 'O'}是消息内容，最后的/dev/null 2>&1是丢进黑洞，因为ros2本身会回显一个已发送，直接扔黑洞，保持屏幕干净
echo "[1.5/4] origin O set (car at launch point = map origin)"
echo "[2/4] SLAM online mapping (FRESH map - builds while driving)..."
setsid bash -c "nohup ros2 launch slam_toolbox online_async_launch.py slam_params_file:=/home/wyk0wjj0gr/slam_params.yaml >> /tmp/slam.log 2>&1 < /dev/null &"  #打开建图
sleep 6
echo "[3/4] online A* planner + continuous lidar following..."
setsid bash -c "nohup python3 -u /home/wyk0wjj0gr/scripts/navigator.py >> /tmp/navigator.log 2>&1 < /dev/null &"  #开启大脑
sleep 2
echo "[4/4] arming + origin..."
if [ -n "$PORT" ]; then   # “PORT变量非空”。第4段找串口时如果压根没找到车模块，这里整个跳过，串口都没有，武装个寂寞，还避免ros2 pub空等。
  ros2 topic pub --once /car_cmd std_msgs/msg/String "{data: 'X'}" > /dev/null 2>&1
  sleep 0.5
  for i in 1 2 3; do
    ros2 topic pub --once /car_cmd std_msgs/msg/String "{data: 'E1'}" > /dev/null 2>&1
    sleep 0.4
  done
fi
if [ "$1" = "rviz" ]; then    #只有输入rviz作为参数的时候，才会打开rviz,避免浪费cpu
  : > /tmp/rviz.log
  nohup env DISPLAY=:0 \     #SSH里启动的图形程序默认不知道往哪画。:0是Pi物理屏幕的显示器编号，rviz试图连显示服务器失败直接死，任何显示都得告诉X服务器
    XAUTHORITY=/home/wyk0wjj0gr/.Xauthority \  #桌面会话只允许持卡者画图，SSH新会话没有默认凭证，必须显式指出用户Xauthority的位置，否则无法画图
    LIBGL_ALWAYS_SOFTWARE=1 \           #强制软件渲染，Pi无可用GPU加速软件，所以渲染慢一点但稳，防止直接崩溃
    rviz2 -d /home/wyk0wjj0gr/scan.rviz \      
    >> /tmp/rviz.log 2>&1 < /dev/null &
  RVIZ_PID=$!
  sleep 3
  if kill -0 "$RVIZ_PID" 2>/dev/null; then      #是个巧妙的用法，信号0什么也不干，纯测试这个进程存在吗，活着返回成功，死了返回失败，2>/dev/null 把没有这个进程的报错吞掉
    echo "RViz started on the Pi desktop (:0), pid=$RVIZ_PID"   #活着，给PID
  else
    echo "RViz failed to start; last log lines:"   #死了，写遗言
    tail -n 20 /tmp/rviz.log
  fi
else
  echo "RViz not requested; use: bash ~/nav.sh rviz"
fi
echo "发车完成: bash ~/goto.sh X Y   (边走边建图, online A* + lidar 连续避障)"

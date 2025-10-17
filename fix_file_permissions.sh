#!/bin/bash

# 修复 /data/misc/cameraserver/ 目录下文件的权限和SELinux上下文
# 使用方法: ./fix_file_permissions.sh

MONITOR_PATH="/data/misc/cameraserver/"

echo "开始修复文件权限和SELinux上下文..."

# 1. 确保目录存在且权限正确
if [ ! -d "$MONITOR_PATH" ]; then
    echo "创建目录: $MONITOR_PATH"
    mkdir -p "$MONITOR_PATH"
fi

# 2. 设置目录权限和SELinux上下文
echo "设置目录权限..."
chmod 777 "$MONITOR_PATH"
chown cameraserver:cameraserver "$MONITOR_PATH"
chcon -R u:object_r:cameraserver_data_file:s0 "$MONITOR_PATH"

# 3. 修复目录下所有文件的权限
echo "修复文件权限..."
for file in "$MONITOR_PATH"*; do
    if [ -f "$file" ]; then
        echo "处理文件: $file"
        
        # 设置文件权限
        chmod 666 "$file"
        chown cameraserver:cameraserver "$file"
        chcon u:object_r:cameraserver_data_file:s0 "$file"
        
        echo "  - 权限已设置为: $(ls -lZ "$file")"
    fi
done

echo "权限修复完成！"
echo "当前目录权限:"
ls -laZ "$MONITOR_PATH"

echo ""
echo "使用说明:"
echo "1. 将文件传输到 $MONITOR_PATH 后，运行此脚本"
echo "2. 或者使用以下命令手动设置单个文件:"
echo "   chmod 666 /data/misc/cameraserver/your_file.jpg"
echo "   chown cameraserver:cameraserver /data/misc/cameraserver/your_file.jpg"
echo "   chcon u:object_r:cameraserver_data_file:s0 /data/misc/cameraserver/your_file.jpg"
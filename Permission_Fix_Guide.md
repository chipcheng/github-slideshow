# ImageInjector 文件权限修复指南

## 问题描述

当文件传输到 `/data/misc/cameraserver/` 目录时，文件权限为 root，但 `cameraserver` 需要以下权限才能访问：

```
-rw-rw-rw-  1 cameraserver cameraserver u:object_r:cameraserver_data_file:s0
```

## 解决方案

### 方案1：使用脚本自动修复（推荐）

1. **上传修复脚本到设备**
```bash
adb push fix_file_permissions.sh /data/local/tmp/
adb shell chmod +x /data/local/tmp/fix_file_permissions.sh
```

2. **运行修复脚本**
```bash
adb shell su -c "/data/local/tmp/fix_file_permissions.sh"
```

3. **验证权限**
```bash
adb shell ls -laZ /data/misc/cameraserver/
```

### 方案2：代码自动修复

ImageInjector 现在会自动检测并修复文件权限：

```cpp
// 在发现文件时自动调用
if (fixFilePermissions(fullPath)) {
    imageFiles.push_back(fullPath);
    ALOGI("Found image file: %s", fullPath.c_str());
} else {
    ALOGW("Skipping file due to permission issues: %s", fullPath.c_str());
}
```

### 方案3：手动修复单个文件

```bash
# 设置文件权限
adb shell su -c "chmod 666 /data/misc/cameraserver/your_file.jpg"

# 设置文件所有者
adb shell su -c "chown cameraserver:cameraserver /data/misc/cameraserver/your_file.jpg"

# 设置 SELinux 上下文
adb shell su -c "chcon u:object_r:cameraserver_data_file:s0 /data/misc/cameraserver/your_file.jpg"
```

### 方案4：使用 C++ 工具

1. **编译权限修复工具**
```bash
# 在 Android 源码目录下
mm -j8 file_permission_fixer
```

2. **推送到设备并运行**
```bash
adb push out/target/product/xxx/system/bin/file_permission_fixer /data/local/tmp/
adb shell su -c "/data/local/tmp/file_permission_fixer"
```

### 方案5：SELinux 策略修复

1. **编译 SELinux 策略**
```bash
# 在 Android 源码目录下
make cameraserver_file_access.pp
```

2. **加载策略**
```bash
adb push cameraserver_file_access.pp /data/local/tmp/
adb shell su -c "semodule -i /data/local/tmp/cameraserver_file_access.pp"
```

## 权限说明

### 文件权限
- **666 (rw-rw-rw-)**：所有用户可读写
- **所有者**：cameraserver:cameraserver (UID:GID = 1000:1000)

### SELinux 上下文
- **类型**：cameraserver_data_file
- **完整上下文**：u:object_r:cameraserver_data_file:s0

## 验证方法

### 1. 检查文件权限
```bash
adb shell ls -laZ /data/misc/cameraserver/
```

期望输出：
```
-rw-rw-rw-  1 cameraserver cameraserver u:object_r:cameraserver_data_file:s0 inject_test.jpg
```

### 2. 检查 cameraserver 是否能访问
```bash
adb shell su -c "run-as cameraserver ls -la /data/misc/cameraserver/"
```

### 3. 查看 ImageInjector 日志
```bash
adb logcat | grep ImageInjector
```

期望看到：
```
I/ImageInjector: Found image file: /data/misc/cameraserver/inject_test.jpg
I/ImageInjector: File permissions after fix: mode=666, uid=1000, gid=1000
I/ImageInjector: File is now readable by cameraserver
```

## 常见问题

### 1. SELinux 上下文设置失败
**原因**：需要 root 权限或适当的 SELinux 策略
**解决**：使用 `su -c` 运行命令，或加载 SELinux 策略

### 2. 文件所有者设置失败
**原因**：目标用户不存在或权限不足
**解决**：确保 cameraserver 用户存在，或使用 root 权限

### 3. 权限设置后仍然无法访问
**原因**：SELinux 策略限制
**解决**：检查 SELinux 状态，加载适当的策略

### 4. 自动修复不工作
**原因**：ImageInjector 没有足够权限
**解决**：确保 ImageInjector 以适当权限运行，或使用手动修复

## 最佳实践

1. **文件传输后立即修复权限**
2. **使用脚本批量处理**
3. **定期检查权限状态**
4. **监控 SELinux 日志**
5. **测试 cameraserver 访问**

## 监控和调试

### 查看 SELinux 拒绝日志
```bash
adb shell dmesg | grep "avc:"
```

### 查看文件系统权限
```bash
adb shell getenforce
adb shell ls -Z /data/misc/cameraserver/
```

### 测试文件访问
```bash
adb shell su -c "run-as cameraserver cat /data/misc/cameraserver/test_file.jpg"
```

这个解决方案确保了 ImageInjector 能够正确访问和处理 `/data/misc/cameraserver/` 目录下的文件。
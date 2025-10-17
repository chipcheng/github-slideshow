/*
 * 文件权限修复工具
 * 用于修复 /data/misc/cameraserver/ 目录下文件的权限和 SELinux 上下文
 */

#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <android/log.h>
#include <selinux/selinux.h>
#include <selinux/label.h>

#define LOG_TAG "FilePermissionFixer"
#define ALOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define ALOGW(...) __android_log_print(ANDROID_LOG_WARN, LOG_TAG, __VA_ARGS__)
#define ALOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

class FilePermissionFixer {
public:
    static bool fixFilePermissions(const char* filePath) {
        ALOGI("Fixing permissions for: %s", filePath);
        
        // 1. 检查文件是否存在
        if (access(filePath, F_OK) != 0) {
            ALOGE("File does not exist: %s", filePath);
            return false;
        }
        
        // 2. 设置文件权限为 666 (rw-rw-rw-)
        if (chmod(filePath, 0666) != 0) {
            ALOGE("Failed to set file permissions: %s", strerror(errno));
            return false;
        }
        ALOGI("File permissions set to 666");
        
        // 3. 设置文件所有者为 cameraserver (UID/GID = 1000)
        if (chown(filePath, 1000, 1000) != 0) {
            ALOGW("Failed to set file ownership: %s", strerror(errno));
            // 继续执行，即使所有权设置失败
        } else {
            ALOGI("File ownership set to cameraserver:cameraserver");
        }
        
        // 4. 设置 SELinux 上下文
        const char* selinuxContext = "u:object_r:cameraserver_data_file:s0";
        
        if (setfilecon(filePath, selinuxContext) != 0) {
            ALOGW("Failed to set SELinux context: %s", strerror(errno));
            ALOGW("This may require root privileges or proper SELinux policy");
        } else {
            ALOGI("SELinux context set to: %s", selinuxContext);
        }
        
        // 5. 验证权限设置
        struct stat fileStat;
        if (stat(filePath, &fileStat) == 0) {
            ALOGI("Final file permissions: mode=%o, uid=%d, gid=%d", 
                  fileStat.st_mode & 0777, fileStat.st_uid, fileStat.st_gid);
            
            // 检查是否可读
            if (access(filePath, R_OK) == 0) {
                ALOGI("File is now readable by cameraserver");
                return true;
            } else {
                ALOGE("File is still not readable after permission fix");
                return false;
            }
        } else {
            ALOGE("Failed to stat file after permission fix: %s", strerror(errno));
            return false;
        }
    }
    
    static bool fixDirectoryPermissions(const char* dirPath) {
        ALOGI("Fixing directory permissions for: %s", dirPath);
        
        // 1. 确保目录存在
        if (access(dirPath, F_OK) != 0) {
            if (mkdir(dirPath, 0777) != 0) {
                ALOGE("Failed to create directory: %s", strerror(errno));
                return false;
            }
            ALOGI("Directory created: %s", dirPath);
        }
        
        // 2. 设置目录权限为 777
        if (chmod(dirPath, 0777) != 0) {
            ALOGE("Failed to set directory permissions: %s", strerror(errno));
            return false;
        }
        ALOGI("Directory permissions set to 777");
        
        // 3. 设置目录所有者为 cameraserver
        if (chown(dirPath, 1000, 1000) != 0) {
            ALOGW("Failed to set directory ownership: %s", strerror(errno));
        } else {
            ALOGI("Directory ownership set to cameraserver:cameraserver");
        }
        
        // 4. 设置 SELinux 上下文
        const char* selinuxContext = "u:object_r:cameraserver_data_file:s0";
        
        if (setfilecon(dirPath, selinuxContext) != 0) {
            ALOGW("Failed to set directory SELinux context: %s", strerror(errno));
        } else {
            ALOGI("Directory SELinux context set to: %s", selinuxContext);
        }
        
        return true;
    }
    
    static bool fixAllFilesInDirectory(const char* dirPath) {
        ALOGI("Fixing all files in directory: %s", dirPath);
        
        // 首先修复目录权限
        if (!fixDirectoryPermissions(dirPath)) {
            ALOGE("Failed to fix directory permissions");
            return false;
        }
        
        // 打开目录
        DIR* dir = opendir(dirPath);
        if (!dir) {
            ALOGE("Failed to open directory: %s", strerror(errno));
            return false;
        }
        
        int fixedCount = 0;
        int totalCount = 0;
        
        // 遍历目录中的所有文件
        struct dirent* entry;
        while ((entry = readdir(dir)) != nullptr) {
            // 跳过 . 和 .. 目录
            if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
                continue;
            }
            
            // 构建完整文件路径
            char fullPath[PATH_MAX];
            snprintf(fullPath, sizeof(fullPath), "%s/%s", dirPath, entry->d_name);
            
            totalCount++;
            
            // 修复文件权限
            if (fixFilePermissions(fullPath)) {
                fixedCount++;
            } else {
                ALOGW("Failed to fix permissions for: %s", fullPath);
            }
        }
        
        closedir(dir);
        
        ALOGI("Permission fix completed: %d/%d files fixed", fixedCount, totalCount);
        return fixedCount > 0;
    }
};

// 使用示例
extern "C" {
    int fix_cameraserver_files() {
        const char* monitorPath = "/data/misc/cameraserver/";
        
        ALOGI("Starting cameraserver file permission fix...");
        
        if (FilePermissionFixer::fixAllFilesInDirectory(monitorPath)) {
            ALOGI("File permission fix completed successfully");
            return 0;
        } else {
            ALOGE("File permission fix failed");
            return -1;
        }
    }
}
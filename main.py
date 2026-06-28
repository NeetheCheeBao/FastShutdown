import os
import win32com.client
import winreg

def get_desktop_path():
    """通过 Windows 注册表获取绝对正确的用户桌面路径"""
    key = winreg.OpenKey(
        winreg.HKEY_CURRENT_USER,
        r"Software\Microsoft\Windows\CurrentVersion\Explorer\User Shell Folders"
    )
    try:
        # Desktop 键值通常为 %USERPROFILE%\Desktop，QueryValueEx 会自动解析环境变量
        desktop_path, _ = winreg.QueryValueEx(key, "Desktop")
        return os.path.expandvars(desktop_path)
    except Exception:
        # 备用方案：如果注册表读取失败，采用标准路径
        return os.path.join(os.environ['USERPROFILE'], 'Desktop')
    finally:
        winreg.CloseKey(key)

def create_shortcut(desktop_folder, name, target, arguments, icon_path, icon_index):
    """创建快捷方式的核心函数"""
    shell = win32com.client.Dispatch("WScript.Shell")
    shortcut_path = os.path.join(desktop_folder, f"{name}.lnk")
    
    shortcut = shell.CreateShortCut(shortcut_path)
    shortcut.TargetPath = target       # 目标程序
    shortcut.Arguments = arguments     # 参数
    shortcut.WorkingDirectory = r"C:\Windows\System32" # 起始位置
    shortcut.IconLocation = f"{icon_path},{icon_index}" # 图标路径和索引
    shortcut.save()

def main():
    desktop = get_desktop_path()
    system32_dir = r"C:\Windows\System32"
    shell32_dll = os.path.join(system32_dir, "shell32.dll")
    
    # 1. 创建“一键关机”快捷方式
    create_shortcut(
        desktop_folder=desktop,
        name="一键关机",
        target=os.path.join(system32_dir, "shutdown.exe"),
        arguments="-s -f -t 0",
        icon_path=shell32_dll,
        icon_index=27
    )
    
    # 2. 创建“一键重启”快捷方式
    create_shortcut(
        desktop_folder=desktop,
        name="一键重启",
        target=os.path.join(system32_dir, "shutdown.exe"),
        arguments="-r -f -t 0",
        icon_path=shell32_dll,
        icon_index=238
    )

    # 弹出提示窗口
    shell = win32com.client.Dispatch("WScript.Shell")
    # Popup 参数说明: Popup(提示内容, 自动关闭秒数(0为不自动关闭), 窗口标题, 按钮及图标类型)
    # 64 表示显示蓝色信息图标（Information），0 表示只显示一个“确定”按钮
    shell.Popup(f"快捷方式已成功创建到桌面！\n路径：{desktop}", 0, "创建成功", 64)

if __name__ == "__main__":
    main()
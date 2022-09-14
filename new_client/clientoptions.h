#ifndef _CLIENTOPTIONS_H_
#define _CLIENTOPTIONS_H_

//进行的操作
enum option
{
    U_LOGIN = 1,
    U_REGISTER,
    U_CHATHALL,
    U_QUITCHAT,
    U_PRIVATE_CHAT,
    U_VIEW_ONLINE,
    U_UPLOAD_FILE,
    U_DOWN_FILE,
    U_UPDATE_NAME,
    U_RETRIEVE_PASSWD,
    U_UPDATE_PASSWD,
    ADMIN_BANNING,
    ADMIN_DIS_BANNING,
    ADMIN_EXITUSR,
    ADMIN_RET,
    HEARTBEAT
    
    /**
     * opt = U_LOGIN --- 登录
     * opt = U_REGISTER --- 注册
     * opt = U_CHATHALL --- 聊天大厅
     * opt = U_QUITCHAT --- 退出聊天大厅
     * opt = U_PRIVATE_CHAT --- 客户端间私聊
     * opt = U_VIEW_ONLINE --- 查看在线用户
     * opt = U_UPLOAD_FILE --- 上传文件
     * opt = U_DOWN_FILE --- 下载文件
     * opt = U_UPDATE_NAME --- 修改昵称
     * opt = U_RETRIEVE_PASSWD --- 找回密码
     * opt = U_UPDATE_PASSWD --- 修改密码
     *
     *
     * 管理员操作
     * opt = ADMIN_BANNING
     * opt = ADMIN_DIS_BANNING
     * opt = ADMIN_EXITUSR
     */
};

#endif // end of clientoptions.h
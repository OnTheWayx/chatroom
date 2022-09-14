//客户端选择的操作以及所需传递的信息
#ifndef _INFO_H_
#define _INFO_H_

#include "userinfo.h"
#include "clientoptions.h"

#define BUF_MAXSIZE 128
struct info
{
    /**
     * @brief 传递的消息正文
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
    //用户信息
    struct usrinfo usr;
    //消息正文
    char text[BUF_MAXSIZE];
    /**
     * @brief 标志客户端进行了什么操作
     * 1 --- 登录
     * 2 --- 注册
     * 3 --- 进入聊天大厅
     * 4 --- 退出聊天大厅
     * 5 --- 在聊天大厅内进行私聊
     * 6 --- 查看在线用户
     * 7 --- 客户端上传文件
     * 8 --- 客户端下载文件
     * 9 --- 修改昵称
     * 10 --- 用户找回密码
     * 11 --- 修改密码
     *
     * 管理员操作
     * 12 --- 对客户端禁言
     * 13 --- 对客户端解除禁言
     * 14 --- 对客户端踢出聊天室
     */
    enum option opt;
};

#endif  // end of info.h
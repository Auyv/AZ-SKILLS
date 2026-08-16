/**
 * @addtogroup cmdline
 * cmdline
 * @{
 */
/**
 * @file        cmdline.h
 * @brief       XXXX
 * @note        XXXX
 * @author      靳普诏(puzhao.jin@hopechart.com)
 * @date        2025/02/07
 * @version     1.0

 * @par         修改日志
 * <table>
 * <tr><th>Date         <th>Version     <th>Author      <th> Description
 * <tr><td>2025/02/07   <td>1.0         <td>靳普诏       <td> 创建初始版本
 * @copyright 杭州鸿泉物联网技术股份有限公司
 */

#ifndef _CMDLINE_H_
#define _CMDLINE_H_


#ifdef __cplusplus
extern "C"
{
#endif

/**
 * hqupgrade命令行处理
 * 
 * 本地无升级包加密
 * hqupgrade --img /media/card/nfxx_upgrade.img
 * 
 * 本地有升级包加密
 * hqupgrade --img /media/card/nfxx_upgrade.img --keys-dir /media/card/keys 
 * 
 * 在线无加密升级包
 * hqupgrade --url URL_ADDRESS92.168.1.100/nfxx_upgrade.img
 * 
 * 在线有加密升级包
 * hqupgrade --url URL_ADDRESS92.168.1.100/nfxx_upgrade.img --keys-dir /media/card/keys
 */

int CmdLine(int argc, char *argv[]);



//---------------------------------------------------------------

/**
 * 下载升级文件包并解压
 * @param *url
 * @param *keys_dir
 * @return 
 * @note:xxxxx
 */
int CmdDownloadImg(const char *url, const char *keys_dir);


#ifdef __cplusplus
}  ///< extern "C"
#endif

#endif  ///< _CMDLINE_H_

/**
 * @}  Generated on "2025-02-07 11:33:59" by the tool "gen_hq_file.py >> V20231119" 
 */


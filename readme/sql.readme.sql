-- -------------------------------------------------------------
-- TablePlus 6.8.0(654)
--
-- https://tableplus.com/
--
-- Database: pc.db
-- Generation Time: 2026-01-13 09:46:14.8560
-- -------------------------------------------------------------


CREATE TABLE "devices" (
    id INTEGER PRIMARY KEY
  , name TEXT
  , ip TEXT
  , groups_id INTEGER DEFAULT(1)
  , mac TEXT
  , status TEXT DEFAULT('离线')
  , last_seen INTEGER
  , remark TEXT
  , type INTEGER
  , hotpot INTEGER
);

CREATE TABLE "policies" (
    id INTEGER PRIMARY KEY
  , groups_id INTEGER
  , "type" TEXT
  , "from_at" TEXT
  , "end_at" TEXT
);

CREATE TABLE "temporary" (
        id INTEGER NOT NULL PRIMARY KEY,
        groups_id INTEGER,
        from_at INTEGER,
        "end_at" INTEGER
);

CREATE TABLE "groups" (
        "id" integer,
        "name" varchar,
        "remark" varchar, 
        "adguard_client_key" varchar, 
        "kind" integer, 
        PRIMARY KEY (id));

CREATE TABLE "holidays" (
        "id" integer,
        "date" text,
        "name" text, 
        PRIMARY KEY (id));

CREATE TABLE "adguard" (
        "id" integer,
        "stat_dt" datetime,
        "device_id" integer,
        "domain" text,
        "query_count" integer,
        "block_count" integer DEFAULT '0', 
        PRIMARY KEY (id));

CREATE TABLE adguard_device_logs (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    device_id INTEGER NOT NULL,
    access_time TEXT NOT NULL,
    domain TEXT NOT NULL,
    reason TEXT,
    is_blocked INTEGER DEFAULT 0, query_type TEXT,
    FOREIGN KEY (device_id) REFERENCES devices(id)
);

CREATE TABLE sys_messages (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    type TEXT NOT NULL,           -- 消息类型: security(撞墙), system(系统), online(上线)
    level TEXT DEFAULT 'info',    -- 级别: info, warning, critical
    source_ip TEXT,               -- 来源 IP
    source_mac TEXT,              -- 来源 MAC
    content TEXT,                 -- 消息正文
    payload TEXT,                 -- JSON 格式的扩展信息 (如 {"target": "google.com"})
    occurrence_count INTEGER DEFAULT 1, -- 发生次数，用于聚合
    is_read BOOLEAN DEFAULT 0,    -- 是否已读
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    updated_at DATETIME DEFAULT CURRENT_TIMESTAMP
);

-- 创建索引提高查询效率
CREATE INDEX idx_msg_type ON sys_messages(type);
CREATE INDEX idx_msg_is_read ON sys_messages(is_read);


DB_PATH = "/opt/parent-control/backend/data/pc.db" 


@@命令和作用
/etc/init.d/pc status,查看服务是否正在运行
/etc/init.d/pc stop,停止服务
/etc/init.d/pc restart,重启服务（修改代码后执行）
logread -f,实时查看系统日志（包括 Python 报出的错误）

@@查看 parent_control 表下的所有集合内容
nft list table inet parent_control

@@查看特定某一个组
nft list set inet parent_control study
@@清空整个 study 集合
nft flush set inet parent_control study
@@删除整个表
nft delete table inet parent_control
@@删除特定 MAC 地址
nft delete element inet parent_control study { c6:ac:91:8f:b6:87 }

@@显示链号 查找特定设备的规则
nft -a list table inet parent_control

@@删除链 带链号
nft delete rule inet parent_control forward handle 979
验证码： E320-925C-7C33-40AF
09DF-7384-E195-4458
@@新的指纹系统
由以下三项组合后进行 SHA256 哈希生成：
block info /dev/mmcblk0p2 获取的 UUID
/sys/class/net/eth0/address 获取Mac地址
/proc/cpuinfo 获取中的Serial

@@checkin
缺少："meta_info":{}

@@服务相关
* 服务路径 ：/etc/systemd/system/api.service
        # 重新加载配置
        sudo systemctl daemon-reload
        # 启动服务
        sudo systemctl start api
        # 设置开机自动启动
        sudo systemctl enable api
* 查看状态：sudo systemctl status api
* 重启（修改代码后执行）：sudo systemctl restart api
* 查看日志：sudo journalctl -u api -f

chain filter_forward { # handle 1
        type filter hook forward priority filter; policy accept;
        ether saddr @temp jump ch_temp # handle 975
        ether saddr @study jump ch_study # handle 977
        ether saddr @fun jump ch_fun # handle 979
        ether saddr @open jump ch_open # handle 981
    }

@@nft基础配置
# 1. 创建存放 timed 组设备的集合
nft add set inet parent_control timed { type ether_addr \; }

# 2. 创建 ch_timed 链
nft add chain inet parent_control ch_timed

# 3. 在 filter_forward 中挂载：属于 timed 组的流量必须进入 ch_timed 链
nft add rule inet parent_control filter_forward ether saddr @timed jump ch_timed

# 4. 在 ch_timed 链的【最后一条】设置默认丢弃
# 注意：我们要给这条规则加个 handle(句柄)，方便识别，或者直接让它永远排在最后
nft add rule inet parent_control ch_timed drop

@@树莓派OS修改时区
# 1. 设置时区名称
uci set system.@system[0].zonename='America/New_York'
# 2. 设置时区格式 (美东时间对应的时区字符串)
uci set system.@system[0].timezone='EST5EDT,M3.2.0,M11.1.0'
# 3. 提交配置并重启系统服务
uci commit system
/etc/init.d/system reload


table inet parent_control {
        set authorized_macs {
                type ether_addr
                elements = { 08:31:8b:69:68:59,
                             08:91:a3:48:28:47,
                             12:5d:1a:d8:29:61,
                             58:00:e3:fb:fc:c9,
                             60:35:73:5d:b5:f5,
                             a0:99:9b:af:b7:11,
                             a6:ba:70:0a:7e:e6,
                             c2:b1:19:54:f5:09,
                             d4:36:39:ab:4e:77,
                             ea:53:0a:f7:1d:3a,
                             ea:da:b9:ce:51:dc,
                             ee:43:c9:8e:99:ff,
                             ee:70:4f:ea:ad:d5,
                             f8:4e:58:10:47:d0 }
        }

        set no_hotspot {
                type ether_addr
                elements = { ee:70:4f:ea:ad:d5 }
        }

        set temp {
                type ether_addr
        }

        set study {
                type ether_addr
                elements = { 12:5d:1a:d8:29:61,
                             58:00:e3:fb:fc:c9 }
        }

        set fun {
                type ether_addr
                elements = { a0:99:9b:af:b7:11,
                             a6:ba:70:0a:7e:e6,
                             f8:4e:58:10:47:d0 }
        }

        set open {
                type ether_addr
                elements = { 08:91:a3:48:28:47,
                             60:35:73:5d:b5:f5,
                             c2:b1:19:54:f5:09,
                             d4:36:39:ab:4e:77,
                             ea:53:0a:f7:1d:3a,
                             ea:da:b9:ce:51:dc,
                             ee:43:c9:8e:99:ff }
        }

        set timed {
                type ether_addr
                elements = { 08:31:8b:69:68:59,
                             ee:70:4f:ea:ad:d5 }
        }

        chain filter_input {
                type filter hook input priority filter; policy accept;
                udp dport 53 ether saddr != @authorized_macs log prefix "REJECT_DNS: " drop
        }

        chain filter_forward {
                type filter hook forward priority filter; policy drop;
                ct state established,related accept
                ether saddr @authorized_macs jump auth_logic
                limit rate 1/minute burst 5 packets log prefix "REJECT_UNAUTHORIZED: " drop
        }

        chain auth_logic {
                ether saddr @no_hotspot ip ttl set 64
                ether saddr @no_hotspot ip ttl <= 63 drop
                ether saddr @temp jump ch_temp
                ether saddr @study jump ch_study
                ether saddr @fun jump ch_fun
                ether saddr @open jump ch_open
                ether saddr @timed jump ch_timed
        }

        chain ch_temp {
                udp dport 53 ip daddr != 192.168.0.8 drop
                udp dport != { 53, 123, 443 } drop
                ip protocol { gre, esp, ah } drop
                tcp dport { 1194, 1723 } drop
                limit rate 1/minute burst 5 packets log prefix "REJECT_TIMEOUT_TEMP: " drop
        }

        chain ch_study {
                udp dport 53 ip daddr != 192.168.0.8 drop
                udp dport != { 53, 123, 443 } drop
                ip protocol { gre, esp, ah } drop
                tcp dport { 1194, 1723 } drop
                limit rate 1/minute burst 5 packets log prefix "REJECT_TIMEOUT_STUDY: " drop
        }

        chain ch_fun {
                limit rate 1/minute burst 5 packets log prefix "REJECT_TIMEOUT_FUN: " drop
        }

        chain ch_open {
                accept
        }

        chain ch_timed {
                udp dport 53 ip daddr != 192.168.0.8 drop
                udp dport != { 53, 123, 443 } drop
                ip protocol { gre, esp, ah } drop
                tcp dport { 1194, 1723 } drop
                limit rate 1/minute burst 5 packets log prefix "REJECT_TIMEOUT_TIMED: " drop
        }
}


在树莓派系统上部署旁路由，实现一个完整、可视化、自动化、稳定、可维护的家庭网络管控系统
操作系统： ImmortalWrt 24.10.4, r33602-e717d133ed6d
主路由： 192.168.0.1
旁路由： 192.168.0.8， 并启用dhcp功能
实现功能如下：
1、VPN阻断（包括ipsec, l2tp, pptp等） 
2、设备用户组分：娱乐组，学习组，开放组，临时组，新设备初次进入自动进入临时组，临时组的设备都禁止上网，开放组不进行任何限制。
3、娱乐组和学习组的设备禁止使用vpn
4、设备除Mac地址外，还需记录设备名称和设备备注
5、web前后端管理，自动适配手机用户
6、AdGuard 日志整合
7、DNS 层 + 流量层 双层统计
8、nftables 网页提交后自动规则生成

web前后端实现功能如下：
1、布局采用左边菜单，右侧内容
2、菜单包括，设备管理、设备组管理，节假日管理，上网策略，临时授权，访问统计
        2.1、设备管理包括设备的增删改，表单包括：设备名称、Mac地址、设备备注，是否在线（设备连接wifi后表示在线，断开wifi表示离线）
        2.2、设备组（娱乐组，学习组，开放组，临时组）管理，可以把没在设备组的设备添加到组中，或者已在组中的设备移除。
        2.3、节假日管理包括节假日的新增或修改
        2.4、上网策略主要功能是定义每个设备组的上网时间段，固定3种时间段分别为：工作日、周末、节假日，一个设备组可以定义多个时间段，比如学习组：工作日16:00-17:00，19:00-21:30等，提交完后自动更新ntf的防火墙规则，及时生效
        2.5、临时授权，可以定义娱乐组或学习组临时开放上网策略： 30分钟，60分钟，120分钟，180分钟，300分钟，倒计时结束自动断网，ntf防火墙即刻生效
        2.6、访问统计，针对各个设备每天的应用统计，网站域名统计等
        2.7、登陆页面增加验证码功能，用户名和密码写入配置文件中，初始用户名和密码分别为：bastonly, LiuY@771106

nft add rule inet pc forward ether saddr 42:5B:3B:A5:AB:B7 drop
curl -X POST -H "Content-Type: application/json" -d '{"name":"iPhone.LY","mac":"42:5B:3B:A5:AB:B7","remark":"LY手机","group":"temp"}' http://192.168.0.8:5000/api/devices

DIST_DIR = "/opt/parent-control/frontend/dist"

@@OpenWRT 备份与恢复（克隆）
备份：dd if=/dev/mmcblk0 bs=4M | gzip > /mnt/sda1/backup_os/ImmortalWrt_24.10.4_$(date +%F).img.gz
恢复：gunzip -c ImmortalWrt_24.10.4_2026-01-28.img.gz | dd of=/dev/sdX bs=4M
@@OpenWRT 克隆到其他USB
dd if=/dev/mmcblk0 of=/dev/sdb bs=4M conv=fsync 把系统克隆到sdb磁盘， 目标盘Size要比源盘大

@@OpenWRT 的各种端口
1、ssh 192.168.0.8:50022 配置文件：/etc/config/dropbear
2、uhttpd http://192.168.0.8:8080/cgi-bin/luci/ 配置文件：/etc/config/uhttpd
3、自研系统Flask http://192.168.0.8:5000/ 配置文件 /opt/parent-control/backend/app.py
4、AdGuard http://192.168.0.8:3000/ 配置文件 /etc/adguardhome.yaml

@@备份系统方法
1、原系统上先备份配置文件 sysupgrade -b /root/backup_os/ImmortalWrt24.10.5-upgrade_$(date +%F).tar.gz
2、备份数据 tar czvf /root/backup_os/appdata_$(date +%F).tar.gz /www /opt --numeric-owner
3、备份软件列表 d | awk '{print $1}' > /root/backup_os/pkglist
4、备份密钥 cp /root/.ssh/authorized_keys /root/backup_os/authorized_keys

@@还原系统方法
1、用 balenaEtcher.app 克隆ImmortalWrt24.10.5系统到SD卡上，
2、把SD卡插入到原有的ImmortalWrt系统上进行空间的扩充。步骤如下：
   2.1 查看此sd卡的信息 比如： fdisk /dev/sda
   2.2 在 Command (m for help): 提示符下依次操作：
        p：打印分区表。记录下 Device /dev/sda2 的 Start 扇区（比如 147456）。
        d：删除分区。
        选择删除第 2 个分区。
        n：新建分区。
        p：选择主分区 (Primary)。
        分区号必须依然是 2。
        输入刚才记录的 Start 扇区：这是最关键的一步！ 不要直接回车，必须手动输入你记录的那个起始位置。
        按回车 (Enter)：结束扇区默认会选到磁盘最后，直接回车即可。
        N：如果提示 Do you want to remove the signature? [Y/n]，必须输入 N！（输入 Y 你的数据就没了）
        w：将更改写入磁盘 (保存并退出)
   2.3 
        # 1. 强制自检（如果分区已挂载，请先 umount /dev/sda2）
        e2fsck -f /dev/sda2
        # 2. 扩展文件系统
        resize2fs /dev/sda2
3、将上面的备份文件 cp /root/backup_os /mnt/sda2/ 
4、把新卡插入机器并启动设备
5、恢复系统配置 sysupgrade -r /backup_os/ImmortalWrt24.10.5-upgrade_$(date +%F).tar.gz
6、软件安装 
        * opkg update
        * 暂停！！！ xargs opkg install < /backup_os/pkglist (这容易跟系统软件冲突)
        * 分步骤安装
          6.1 Web / 服务类： opkg install nginx-full nginx-ssl-util apache apache-utils
          6.2 Docker 环境：opkg install docker dockerd docker-compose containerd runc tini libseccomp
          6.3 Python 环境：opkg install python3 python3-pip python3-openssl python3-sqlite3 python3-logging python3-urllib python3-lzma python3-multiprocessing python3-ctypes python3-readline python3-uuid python3-xml
          6.4 常用工具： opkg install bash sudo git rsync tcpdump fdisk btrfs-progs e2fsprogs ntfs3-mount
          6.5 文件，存储： opkg install kmod-fs-exfat kmod-fs-ntfs3 kmod-fs-ext4 kmod-usb-storage kmod-usb-storage-uas kmod-usb-net kmod-usb-net-lan78xx
7、恢复数据
        * 先删除/www下的所有文件 rm -rf /www
        * tar xzvf /backup_os/appdata_$(date +%F).tar.gz / --overwrite --overwrite-dir --no-same-permissions
        * 检查链接文件是否存在，若不存在执行下面的操作：
                ln -s /www/default/cgi-bin /www/cgi-bin
                ln -s /www/default/luci-static/ /www/luci-static
8、还原密钥
        * cp /backup_os/authorized_keys /root/.ssh/
9、重新启动系统
10、若要迁回docker数据用下面这种方式：Docker配置文件一般是在 /etc/docker/daemon.json, 但树莓派系统要更改 /etc/config/dockerd文件
        * rsync -aHAX /mnt/sda1/docker/ /opt/docker/
          说明：
          -a → 归档模式，保留权限、时间戳、符号链接
          -H → 保留硬链接（Docker 镜像层有硬链接）
          -A → 保留 ACL
          -X → 保留 xattr（扩展属性，Docker overlay2 需要）
          注意路径末尾 / 很重要，表示 拷贝目录内容到目标，而不是创建 /opt/docker/docker。
        * 备选方式：cp -a ， cp -a /mnt/sda1/docker/. /opt/docker/
          注意 /. 才会把隐藏文件和所有内容一起拷贝
          但 rsync 更安全，可以显示进度并支持中断续传
        * 验证迁移 docker info | grep "Docker Root Dir"



@@Cloudflare
1、域名bastonly.top的API token: NJm4nlzWFEU8DSbUix1McNCAEHRxY5ZOPVtOlwF6
   验证： curl "https://api.cloudflare.com/client/v4/user/tokens/verify" -H "Authorization: Bearer NJm4nlzWFEU8DSbUix1McNCAEHRxY5ZOPVtOlwF6"

@@生成 .sha256 校验文件非常简单，您可以在**打包升级包的服务器（或您的开发机）**上运行以下命令。

1. Linux / macOS (推荐方式)
在您的 update.tar.gz 文件所在的目录下，运行：
bash
# 生成校验文件
sha256sum update.tar.gz > update.tar.gz.sha256
生成的 update.tar.gz.sha256 文件内容通常长这样： e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855  update.tar.gz

2. 如果是 macOS (没有 sha256sum 命令)
可以使用 shasum 工具：
bash
shasum -a 256 update.tar.gz > update.tar.gz.sha256

3. Windows (PowerShell)
如果您在 Windows 上准备包，可以使用：
powershell
Get-FileHash update.tar.gz -Algorithm SHA256 | Format-List
# 然后手动将 Hash 字符串写入 update.tar.gz.sha256 文件中
操作建议：
每次您发布新版本（比如 v1.02）时，流程如下：

打包：tar -czf update.tar.gz frontend backend service
计算哈希：sha256sum update.tar.gz > update.tar.gz.sha256
上传：将这两个文件同时传到您的下载服务器 https://verify.bastonly.top/download/ 目录下。
这样，我们的 
upgrade.sh
脚本在下载完压缩包后，会自动拉取同名的 .sha256 文件进行比对。如果比对一致，才会开始解压安装，非常稳妥。

# -*- coding: gb18030 -*-

# $Id: csdefine.py,v 1.182 2008-09-05 02:23:15 songpeifang Exp $


"""
locates global definations of base/cell/client

2005.06.06 : tidied up by huangyongwei
"""

import cPickle
import Language
import cschannel_msgs
import ShareTexts as ST

# --------------------------------------------------------------------
# 版本控制，决定运行的是绿色版还是普通版
# 这样，我们就可以根据实际情况，区分某些系统或功能
# 在某些需要区分的地方插入对此值的判断
# 0 == 普通版本；1 == 绿色版本
# --------------------------------------------------------------------
IS_GREEN_VERSION = 1

# --------------------------------------------------------------------
# about server state( from L3Define.py；defined by penghuawei )
# --------------------------------------------------------------------
SERVERST_STATUS_COMMON				= 1 	# 状态一般
SERVERST_STATUS_WELL				= 2		# 状态良好
SERVERST_STATUS_BUSY				= 3		# 服务器繁忙
SERVERST_STATUS_HALTED				= 4		# 服务器暂停

# --------------------------------------------------------------------
# 防沉迷
# --------------------------------------------------------------------
# 状态定义
WALLOW_STATE_COMMON					= 0		# 正常状态
WALLOW_STATE_HALF_LUCRE				= 1		# 收益减半
WALLOW_STATE_NO_LUCRE				= 2		# 收益为 0



# --------------------------------------------------------------------
# about initialize type of the role( 2008.06.05: by penghuawei )
# --------------------------------------------------------------------
ROLE_INIT_OPRECORDS					= 0		# 操作记录的初始化
ROLE_INIT_KITBAGS					= 1		# 背包的初始化
ROLE_INIT_ITEMS						= 2		# 物品和道具的初始化
ROLE_INIT_SKILLS					= 3		# 技能列表的初始化
ROLE_INIT_BUFFS						= 4		# buff 的初始化
ROLE_INIT_COMPLETE_QUESTS			= 5		# 完成任务的初始化列表
ROLE_INIT_QUEST_LOGS				= 6		# 任务日志初始化
ROLE_INIT_PETS						= 7		# 宠物初始化
ROLE_INIT_COLLDOWN					= 8		# 冷却
ROLE_INIT_QUICK_BAR					= 9		# 快捷栏初始化
ROLE_INIT_PRESTIGE					= 10	# 初始化声望
ROLE_INIT_VEHICLES					= 11	# 初始化骑宠
ROLE_INIT_MONEY						= 12	# 初始化金钱
ROLE_INIT_OFLMSGS					= 13	# 初始化离线消息




# --------------------------------------------------------------------
# about entity ( from common/utype.py；designed by all team members )
# --------------------------------------------------------------------
ENTITY_TYPE_ROLE					= 1		# 角色
ENTITY_TYPE_NPC						= 2		# NPC
ENTITY_TYPE_MONSTER					= 3		# 怪物
ENTITY_TYPE_PET						= 4		# 宠物
ENTITY_TYPE_PREVIEWROLE				= 5		# 角色选择entity
ENTITY_TYPE_DROPPED_ITEM			= 6		# 掉下物品
ENTITY_TYPE_SPAWN_POINT				= 7		# 出生点
ENTITY_TYPE_SPACE_DOOR				= 8		# 传送门
ENTITY_TYPE_SPACE_TRANSPORT			= 9		# 传送点
ENTITY_TYPE_SPACE_ENTITY			= 10	# 传送师
ENTITY_TYPE_WEATHER_SYSTEM			= 11	# 天气系统
ENTITY_TYPE_QUEST_BOX				= 12	# 任务箱子
ENTITY_TYPE_PROXIMITY_TRANSDUCER	= 13	# ？
ENTITY_TYPE_SPACE_GATE				= 14	# 关卡的门
ENTITY_TYPE_SLAVE_MONSTER			= 15	# 有主人的怪物（譬如护镖NPC）
ENTITY_TYPE_MISC					= 16	# 杂物（其它没有特殊需要判断的类型都归为此类）
ENTITY_TYPE_TREASURE_MONSTER		= 17	# 宝藏类怪物（所有权分配以及经验的获得有别于普通Monster）
ENTITY_TYPE_DROPPED_BOX				= 18	# 掉下物品
ENTITY_TYPE_CONVOY_MONSTER			= 19	# 跟随怪物
ENTITY_TYPE_VEHICLE					= 20	# 骑宠
ENTITY_TYPE_TONG_CITYWAR_MONSTER	= 21	# 帮会夺城战怪物
ENTITY_TYPE_VEHICLE_DART			= 22	# 镖车
ENTITY_TYPE_YAYU					= 23	# 猰貐（暂时没有对可以被攻击的怪物独立一个类型，看日后需求在说）
ENTITY_TYPE_SERVER_ENTITY			= 24	# 服务器ENTITY， 客户端不可见， 支持AI
ENTITY_TYPE_COLLECT_POINT			= 25	# 采集点 by 姜毅
ENTITY_TYPE_TONG_NAGUAL				= 26	# 帮会神兽
ENTITY_TYPE_MONSTER_ATTACK_BOX		= 27	# 怪物攻城活动中掉落的“迷惘之箱”等箱子
ENTITY_TYPE_FRUITTREE				= 28	# 七夕活动中魅力果树
ENTITY_TYPE_EIDOLON_NPC				= 29	# 小精灵NPC
ENTITY_TYPE_CHALLENGE_TRANSDUCER	= 30	# 挑战副本怪物陷阱
ENTITY_TYPE_CALL_MONSTER			= 31	# 召唤类怪物
ENTITY_TYPE_SPACE_CHALLENGE_DOOR	= 32	# 华山阵法传送门
ENTITY_TYPE_CITY_MASTER				= 33	# 城主雕像
ENTITY_TYPE_PANGU_NAGUAL			= 34	# 盘古守护
ENTITY_TYPE_NPC_FORMATION			= 35	# 布阵NPC
ENTITY_TYPE_SKILL_TRAP				= 36    # 技能陷阱
ENTITY_TYPE_NPC_OBJECT				= 37	# 场景物件
ENTITY_TYPE_MONSTER_BELONG_TEAM		= 38	# 根据队伍判断战斗关系怪物





# 用于标识或控制某个Entity(主要是NPC)能拥有哪些功能
ENTITY_FLAG_SKILL_TRAINER				= 1				# 技能训练师
ENTITY_FLAG_CHAPMAN						= 2				# 商人
ENTITY_FLAG_REPAIRER					= 3				# 修理工
ENTITY_FLAG_BANK_CLERK					= 4				# 银行办事员
ENTITY_FLAG_POSTMAN						= 5				# 邮差
ENTITY_FLAG_AUCTIONEER					= 6				# 拍卖师
ENTITY_FLAG_TRANSPORT					= 7				# 传送器
ENTITY_FLAG_QUEST_ISSUER				= 8				# 任务发放者
ENTITY_FLAG_SPEAKER						= 9				# 说话者（注：所有可以对话的NPC、怪物都应该设置此选项，只有置了此值，客户端才应该响应对话，服务器亦会判断；此标志用于未来AI临时关闭NPC的对话功能；暂未实现）
ENTITY_FLAG_QUEST_BOX					= 10			# 任务箱子
ENTITY_FLAG_EQUIP_MAKER					= 11			# 装备制造师
ENTITY_FLAG_PET_CARE					= 12			# 宠物保育员
ENTITY_FLAG_MONSTER_INITIATIVE			= 13
ENTITY_FLAG_CAN_CATCH					= 14			# 可捕捉对象
ENTITY_FLAG_COLLECT_POINT				= 15			# 采集点 by 姜毅
ENTITY_FLAG_COPY_STARTING				= 16			# 副本正在进行中
ENTITY_FLAG_QUEST_STARTING				= 17			# 任务正在进行中
ENTITY_FLAG_QUEST_WORKING				= 18			# NPCOBJECT处于任务表现中
ENTITY_FLAG_CHANG_COLOR_RED				= 19			# npc模型变为红色
ENTITY_FLAG_CAN_NOT_SELECTED			= 20			# 不能被角色鼠标选择
ENTITY_FLAG_CAN_BE_HIT_BY_MONSTER		= 21			# 可以被怪物攻击标志
ENTITY_FLAG_UNVISIBLE					= 22			# 该对象不可见
ENTITY_FLAG_CHRISTMAS_HORSE				= 23			# 圣诞节赛马标志
ENTITY_FLAG_MONSTER_FLY					= 24			# 客户端可以表现monster在空中行走(更换了monster的filter)
ENTITY_FLAG_CANT_BE_HIT_BY_ROLE			= 25			# 不可以被玩家攻击标志
ENTITY_FLAG_GUARD						= 26			# 卫兵
ENTITY_FLAG_CANT_ROTATE_IN_FIGHT_STATE	= 27			# 进入战斗状态不转身标志
ENTITY_FLAG_ONLY_FACE_LEFT_OR_RIGHT		= 28			# 横板游戏怪物朝向标志
ENTITY_FLAG_MONSTER_THINK				= 29			# 怪物不在玩家AOI范围内也可以think的标志
ENTITY_FLAG_NOT_CHECK_AND_MOVE			= 30			# 怪物不会自己主动选择站位的标志
ENTITY_FLAG_NOT_FULL					= 31			# 不回血回蓝标记
ENTITY_FLAG_ALAWAY_SHOW_NAME			= 32			# 不受设置影响，怪物一直显示名字标识
ENTITY_VOLATILE_ALWAYS_OPEN             = 33            # 坐标朝向同步一直打开
ENTITY_FLAG_FRIEND_ROLE             	= 34            # 玩家对怪物友好
# ----------------------------------------------------
# 玩家当前身上拥有的标记
ROLE_FLAG_CAPTAIN						= 1				# 队长
ROLE_FLAG_MERCHANT						= 2				# 跑商状态
ROLE_FLAG_XL_ROBBING					= 3				# 兴隆劫镖任务
ROLE_FLAG_BLOODY_ITEM					= 4				# 携带带血物品
ROLE_FLAG_ICHOR							= 5 			# 灵药秘术状态
ROLE_FLAG_DRIVERING_DART				= 6 			# 在驾驶镖车状态
ROLE_FLAG_SPREADER						= 7 			# 推广员
ROLE_FLAG_XL_DARTING					= 8				# 兴隆运镖者
ROLE_FLAG_CP_DARTING					= 9				# 昌平运镖者
ROLE_FLAG_CP_ROBBING					= 10			# 昌平劫镖任务
ROLE_FLAG_TEAMMING						= 11 			# 组队中
ROLE_FLAG_SPEC_COMPETETE				= 12 			# 进行某特殊竞技中
ROLE_FLAG_FLY_TELEPORT					= 13 			# 飞翔传送
ROLE_FLAG_TISHOU						= 14 			# 替身寄售状态
ROLE_FLAG_TONG_BAG						= 15			# 打开帮会仓库状态 by姜毅
ROLE_FLAG_REGISTER_MASTER				= 16			# 注册收徒标记
ROLE_FLAG_COUPLE_AGREE					= 17			# 玩家同意结婚或离婚标记
ROLE_FLAG_AUTO_FIGHT					= 18			# 玩家进入自动战斗标记
ROLE_FLAG_CITY_WAR_FUNGUS				= 19			# 城战采到蘑菇者的标记
ROLE_FLAG_SAFE_AREA						= 20			# 安全区域
ROLE_FLAG_HIDE_INFO						= 21			# 隐藏身份标志
ROLE_FLAG_AREA_SKILL_ONLY				= 22			# 空间技能标志,意思是指当前只能使用这个空间配置的技能。
ROLE_FLAG_FLY							= 23			# 飞行中

ROLE_FLAG_ROLE_RECORD_INIT_OVER			= 30	#角色初始化完毕roleRecord的标志
ROLE_FLAG_ACCOUNT_RECORD_INIT_OVER		= 31	#角色初始化完毕accountRecord的标志

# --------------------------------------------------------------------
# 角色状态
# --------------------------------------------------------------------
ENTITY_STATE_FREE						= 0				# 自由状态
ENTITY_STATE_DEAD						= 1				# 死亡状态
ENTITY_STATE_REST						= 2				# 休息状态
ENTITY_STATE_FIGHT						= 3				# 战斗状态
ENTITY_STATE_PENDING					= 4				# 未决状态
ENTITY_STATE_HANG						= 5				# 挂起状态(中断交互并隐藏模型)
ENTITY_STATE_VEND						= 6				# 摆摊状态
ENTITY_STATE_RACER						= 7				# 比赛状态（例如：赛马）
ENTITY_STATE_CHANGING					= 8				# 变身状态（如：钓鱼，变身大赛等）
ENTITY_STATE_QUIZ_GAME					= 9				# 问答状态
ENTITY_STATE_DANCE						= 10			# 跳舞状态
ENTITY_STATE_REQUEST_DANCE				= 11			# 邀请跳舞状态
ENTITY_STATE_DOUBLE_DANCE				= 12			# 双人跳舞状态(区分ENTITY_STATE_DANCE，为了客户端播放动作)
ENTITY_STATE_ENVIRONMENT_OBJECT			= 13			# 场景物件状态
ENTITY_STATE_MAX						= 14			# MAX



ACTION_FORBID_MOVE						= 0x00000001	# 不允许移动标志( 原名：ACTION_MOVE )
ACTION_FORBID_CHAT						= 0x00000002	# 不允许聊天标志( 原名：ACTION_CHAT )
ACTION_FORBID_USE_ITEM					= 0x00000004	# 不允许使用物品标志( 原名：ACTION_USE_ITEM )
ACTION_FORBID_WIELD						= 0x00000008	# 不允许装备物品标志( 原名：ACTION_WIELD )
ACTION_FORBID_ATTACK					= 0x00000010	# 不允许攻击标志(普通物理攻击)( 原名：ACTION_ATTACK )
ACTION_FORBID_SPELL_PHY					= 0x00000020	# 不允许使用物理技能标志
ACTION_FORBID_SPELL_MAGIC				= 0x00000040	# 不允许使用法术技能标志
ACTION_FORBID_TRADE						= 0x00000080	# 不允许交易标志( 原名：ACTION_TRADE )
ACTION_FORBID_FIGHT						= 0x00000100	# 不允许进入战斗标志( 原名：ACTION_FIGHT )
ACTION_ALLOW_VEND						= 0x00000200	# 允许摆摊状态
ACTION_FORBID_JUMP						= 0x00000400	# 不允许跳跃状态
ACTION_FORBID_PK						= 0x00000800	# 不允许PK
ACTION_FORBID_CALL_PET					= 0x00001000	# 不允许招宠物
ACTION_FORBID_TALK						= 0x00002000	# 不允许与NPC对话
ACTION_FORBID_BODY_CHANGE				= 0x00004000	# 不允许变身
ACTION_FORBID_VEHICLE					= 0x00008000	# 不允许骑乘
ACTION_ALLOW_DANCE						= 0x00010000	# 允许跳舞状态
ACTION_FORBID_INTONATING				= 0x00020000	# 不允许吟唱
ACTION_FORBID_CHANGE_MODEL				= 0x00040000	# 不充许换装备后更新模型

#组合标志
ACTION_FORBID_SPELL = ACTION_FORBID_SPELL_PHY | ACTION_FORBID_SPELL_MAGIC






# 怪物子状态
M_SUB_STATE_NONE						= 0 		# 无
M_SUB_STATE_FLEE						= 1 		# 逃跑
M_SUB_STATE_CHASE						= 2 		# 追击
M_SUB_STATE_WALK						= 3 		# 随机转悠
M_SUB_STATE_GOBACK						= 4 		# 战斗结束回走
M_SUB_STATE_CONTINUECHASE				= 5   		# 随机走动,用于修复navigateFollow抛出的异常

# entity 姿态
ENTITY_POSTURE_NONE						= 0			# 无姿态
ENTITY_POSTURE_DEFENCE					= 1			# 防御
ENTITY_POSTURE_VIOLENT					= 2			# 狂暴
ENTITY_POSTURE_DEVIL_SWORD				= 3			# 魔剑
ENTITY_POSTURE_SAGE_SWORD				= 4			# 圣剑
ENTITY_POSTURE_SHOT						= 5			# 神射
ENTITY_POSTURE_PALADIN					= 6			# 游侠
ENTITY_POSTURE_MAGIC					= 7			# 法术
ENTITY_POSTURE_CURE						= 8			# 医术


# --------------------------------------------------------------------
# about role( from L3Common )
# --------------------------------------------------------------------
# 性别
GENDER_MALE								= 0x0		# 男性
GENDER_FEMALE							= 0x1		# 女性

# 职业
CLASS_UNKNOWN							= 0x00		# 未知
CLASS_FIGHTER							= 0x10		# 战士
CLASS_SWORDMAN							= 0x20		# 剑客
CLASS_ARCHER							= 0x30		# 射手
CLASS_MAGE								= 0x40		# 法师
CLASS_PALADIN							= 0x50		# 强防型战士（NPC专用）
CLASS_WARLOCK							= 0x60		# 巫师( 已去掉 )
CLASS_PRIEST							= 0x70		# 祭师( 已去掉 )

# entity 阵营
ENTITY_CAMP_NONE						= 0 # 没有阵营教
ENTITY_CAMP_TAOISM						= 1 # 天道阵营
ENTITY_CAMP_DEMON						= 2 # 魔道阵营

# 国家
# phw: 不再有国家，改为真正的种族：人型、动物、值物、妖魔等
YANHUANG								= 0x0000	# 炎黄国
JIULI									= 0x0100	# 九黎国
FENGMING								= 0x0200	# 凤鸣国

# 属性掩码
RCMASK_GENDER							= 0x0000000f	# 性别掩码
RCMASK_CLASS							= 0x000000f0	# 职业掩码
RCMASK_RACE								= 0x00000f00	# 种族掩码
RCMASK_FACTION							= 0x000ff000	# 所属势力
RCMASK_CAMP								= 0x00f00000	# 所属阵营
RCMASK_ALL								= 0x00ffffff	# RCMASK_GENDER | RCMASK_CLASS | RCMASK_RACE | RCMASK_FACTION | RCMASK_CAMP






# 玩家在各种关系中的身份标记( wsf )
TEACH_PRENTICE_FLAG							= 0x001			# 师徒关系中的徒弟身份
TEACH_MASTER_FLAG							= 0x002			# 师徒关系中的师父身份

SWEETIE_IDENTITY							= 0x004			# 恋人关系标记

COUPLE_AGREEMENT							= 0x008			# 同意结为夫妻关系的标记
COUPLE_IDENTITY								= 0x010			# 夫妻关系标记

TEACH_REGISTER_MASTER_FLAG					= 0x020			# 是否注册收徒的标记


# --------------------------------------------------------------------
# 客户端操作记录类型
# --------------------------------------------------------------------
OPRECORD_COURSE_HELP						= 1				# 过程帮助记录类型
OPRECORD_UI_TIPS							= 2				# UI 提示记录
OPRECORD_PIXIE_HELP							= 3				# 随身精灵帮助记录



# --------------------------------------------------------------------
# about npc
# --------------------------------------------------------------------
# NPC的行为( from L3Define.py )
NPC_BEHAVE_STATE_NONE					= 0				# 无
NPC_BEHAVE_STATE_WALK					= 1				# 走
NPC_BEHAVE_STATE_RUN					= 2				# 跑


# --------------------------------------------------------------------
# about pet( hyw )
# --------------------------------------------------------------------
# 宠物的印记
PET_STAMP_SYSTEM						= 1				# 系统印记
PET_STAMP_MANUSCRIPT					= 2				# 手写印记

# 宠物辈分
PET_HIERARCHY_GROWNUP					= 0x01			# 成年宠物
PET_HIERARCHY_INFANCY1					= 0x02			# 一代宠物
PET_HIERARCHY_INFANCY2					= 0x03			# 二代宠物
# 宠物类别
PET_TYPE_STRENGTH						= CLASS_FIGHTER		# 力量型( 对应战士 )
PET_TYPE_BALANCED						= CLASS_SWORDMAN	# 均衡型( 对应剑客 )
PET_TYPE_SMART							= CLASS_ARCHER		# 敏捷型( 对应射手 )
PET_TYPE_INTELLECT						= CLASS_MAGE		# 智力型( 对应法师 )
# 掩码
PET_HIERARCHY_MASK						= 0x0f			# 宠物辈分掩码
PET_TYPE_MASK							= 0xf0			# 宠物类别掩码

# 宠物性格
PET_CHARACTER_SUREFOOTED				= 1				# 稳重的
PET_CHARACTER_CLOVER					= 2				# 聪慧的
PET_CHARACTER_CANNILY					= 3				# 精明的
PET_CHARACTER_BRAVE						= 4				# 勇敢的
PET_CHARACTER_LIVELY					= 5				# 活泼的

# -------------------------------------------
# 宠物获得方式
PET_GET_CATHOLICON						= 1				# 还童丹
PET_GET_RARE_CATHOLICON					= 2				# 洗髓丹
PET_GET_SUPER_CATHOLICON					= 3			# 超级还童丹
PET_GET_SUPER_RARE_CATHOLICON				= 4			# 超级洗髓丹
PET_GET_SUPER_CATCH							= 5			# 万能捕捉
PET_GET_CATCH							= 6				# 捕捉
PET_GET_PROPAGATE						= 7				# 繁殖
PET_GET_EGG1							= 8				# 成年宠物蛋
PET_GET_EGG2							= 9				# 1代宠物蛋
PET_GET_EGG3							= 10			# 2代宠物蛋

# -------------------------------------------
# 宠物状态
PET_STATUS_WITHDRAWED					= 1				# 回收状态
PET_STATUS_WITHDRAWING					= 2				# 正在回收状态
PET_STATUS_CONJURING					= 3				# 正在召唤状态
PET_STATUS_CONJURED						= 4				# 出征状态
# 宠物回收方式
PET_WITHDRAW_COMMON						= 1				# 正常回收
PET_WITHDRAW_HP_DEATH					= 2				# 因血量为零死亡回收
PET_WITHDRAW_LIFE_DEATH					= 3				# 因寿命为零死亡回收
PET_WITHDRAW_CONJURE					= 4				# 出兵回收
PET_WITHDRAW_FREE						= 5				# 放生回收
PET_WITHDRAW_OWNER_DEATH				= 6				# 所属玩家死亡回收
PET_WITHDRAW_OFFLINE					= 7				# 离线回收
PET_WITHDRAW_FLYTELEPORT				= 8				# 飞翔传送回收
PET_WITHDRAW_GMWATCHER					= 9				# 设置GM观察者回收
PET_WITHDRAW_BUFF						= 10			# buff要求回收

# -----------------------------------------------------
# 宠物的移动行为状态
PET_ACTION_MODE_FOLLOW					= 0				# 跟随
PET_ACTION_MODE_KEEPING					= 1				# 停留
# 宠物的战斗行为状态
PET_TUSSLE_MODE_ACTIVE					= 0				# 主动
PET_TUSSLE_MODE_PASSIVE					= 1				# 被动
PET_TUSSLE_MODE_GUARD					= 2				# 防御


# -----------------------------------------------------
# 强化类型
PET_ENHANCE_COMMON						= 1				# 普通强化
PET_ENHANCE_FREE						= 2				# 自由强化
# 魂魄石种类
PET_ANIMA_GEM_TYPE_CRACKED				= 1				# 破碎的
PET_ANIMA_GEM_TYPE_COMMON				= 2				# 普通的
PET_ANIMA_GEM_TYPE_PERFECT				= 3				# 完美的


# -----------------------------------------------------
# 宝石类型
PET_GEM_TYPE_NONE						= 0				# 未知宝石种类
PET_GEM_TYPE_COMMON						= 1				# 普通宝石
PET_GEM_TYPE_TRAIN						= 2				# 代练宝石
# 宠物的代练方式
PET_TRAIN_TYPE_NONE	  					= 0				# 没有代练
PET_TRAIN_TYPE_COMMON					= 1				# 普通代练方式
PET_TRAIN_TYPE_HARD	  					= 2				# 刻苦代练方式

# -----------------------------------------------------
# 仓库类型
PET_STORE_TYPE_LARGE					= 1				# 大仓库
PET_STORE_TYPE_SMALL					= 2				# 小仓库

PET_STORE_STATUS_NONE					= 0				# 未租用状态
PET_STORE_STATUS_HIRING					= 1				# 正在使用状态
PET_STORE_STATUS_OVERDUE				= 2				# 过期状态

# -----------------------------------------------------
# 宠物繁殖状态
PET_PROCREATE_STATUS_NONE				= 0				# 没有繁殖
PET_PROCREATE_STATUS_PROCREATING		= 1				# 正在繁殖
PET_PROCREATE_STATUS_PROCREATED			= 2				# 已繁殖

# -----------------------------------------------------
# 宠物繁殖操作状态
PET_PROCREATION_DEFAULT					= 0				# 默认状态，玩家没进行宠物繁殖操作
PET_PROCREATION_WAITING					= 1				# 等待状态，玩家触发npc对话先进入等待状态
PET_PROCREATION_SELECTING 				= 2				# 选择繁殖宠物状态
PET_PROCREATION_LOCK 					= 3				# 锁定宠物状态
PET_PROCREATION_SURE					= 4				# 确认提交繁殖宠物状态
PET_PROCREATION_COMMIT					= 5				# 提交繁殖宠物状态


# --------------------------------------------------------------------
# about skill( from common/QuestDefine.py；designed by penghuawei )
# --------------------------------------------------------------------
# SKILL的效果类型；将来有可能良性的效果允许玩家手动取消，恶性的和未定义的效果不允许取消
SKILL_EFFECT_STATE_BENIGN				= 1				# 良性效果
SKILL_RAYRING_EFFECT_STATE_BENIGN		= 2				# 光环良性效果
SKILL_EFFECT_STATE_NONE					= 0				# 未定义类型：中庸(无状态)
SKILL_EFFECT_STATE_MALIGNANT			= -1			# 恶性
SKILL_RAYRING_EFFECT_STATE_MALIGNANT	= -2			# 光环恶性
# 施展方式
SKILL_CAST_OBJECT_TYPE_NONE				= 0				# 不需要目标和位置;
SKILL_CAST_OBJECT_TYPE_POSITION			= 1				# 对位置施展;
SKILL_CAST_OBJECT_TYPE_ENTITY			= 2				# 对目标Entity施展;
SKILL_CAST_OBJECT_TYPE_ITEM				= 3				# 对目标物品施展
SKILL_CAST_OBJECT_TYPE_ENTITYS			= 4				# 对多个目标Entity施展;
# 法术作用范围
SKILL_SPELL_AREA_SINGLE					= 0				# 单体
SKILL_SPELL_AREA_CIRCLE					= 1				# 圆形区域
SKILL_SPELL_AREA_RADIAL					= 2				# 直线
SKILL_SPELL_AREA_SECTOR					= 3				# 扇形

# buff类别定义
BUFF_TYPE_NONE							= 0				# 未定义类型
BUFF_TYPE_ATTRIBUTE						= 1				# 基础属性
BUFF_TYPE_PHYSICS_ATTACK				= 2				# 物理攻击性表现属性
BUFF_TYPE_MAGIC_ATTACK					= 3				# 法术攻击表现属性
BUFF_TYPE_NOT_ATTACK					= 4				# 非攻击性表现属性
BUFF_TYPE_RESIST						= 5				# 抗性
BUFF_TYPE_MOVE_SPEED					= 6				# 移动和速度
BUFF_TYPE_SLOW_OPER						= 7				# 缓慢作用效果
BUFF_TYPE_AFFECT_ACTION					= 8				# 影响行为效果
BUFF_TYPE_AFFECT_CURE					= 9				# 治疗效果变化
BUFF_TYPE_BRING_DAMAGE					= 10			# 易伤效果
BUFF_TYPE_SPECILIZE_ACCORD				= 11			# 特殊符合类
BUFF_TYPE_IMMUNE						= 12			# 免疫效果
BUFF_TYPE_DERATE						= 13			# 减免
BUFF_TYPE_DISCHARGE_SKILL				= 14			# 施法
BUFF_TYPE_IMBIBE						= 15			# 吸收
BUFF_TYPE_REFLECT						= 16			# 反射效果
BUFF_TYPE_WEAPON						= 17			# 武器效果
BUFF_TYPE_VAMPIRE						= 18			# 类吸血效果
BUFF_TYPE_RELATION_AWARD				= 19			# 关系奖励效果
BUFF_TYPE_HIDE							= 20			# 隐身效果
BUFF_TYPE_DIFFUSE						= 21			# 扩散效果
BUFF_TYPE_SYSTEM						= 22			# 系统BUFF
BUFF_TYPE_SHIELD1						= 23			# 转化护盾I
BUFF_TYPE_SHIELD2						= 24			# 转化护盾II
BUFF_TYPE_ATTACK_RANGE					= 25			# 射程变化
BUFF_TYPE_ELEM_ATTACK_BUFF				= 26			# 元素攻击
BUFF_TYPE_ELEM_DERATE_RATIO_BUFF		= 27			# 元素抗性
BUFF_TYPE_COMPLEX_BASE_PROPERTY1		= 60			# 基础属性复合I
BUFF_TYPE_COMPLEX_SPEED1				= 61			# 速度复合I
BUFF_TYPE_COMPLEX_ATTACK				= 62			# 攻击复合
BUFF_TYPE_COMPLEX_SHIELD				= 63			# 护盾类复合
BUFF_TYPE_COMPLEX_SPEED2				= 64			# 速度复合II
BUFF_TYPE_COMPLEX_BIDIRECTIONAL1		= 65			# 双向复合I
BUFF_TYPE_COMPLEX_BASE_PROPERTY2		= 66			# 基础属性复合II
BUFF_TYPE_FLAW							= 70			# 破绽效果
BUFF_TYPE_COMPLEX_PUBLIC				= 99			# 公共效果
BUFF_TYPE_ELEM_ATTACK_DEBUFF			= 126			# 元素攻击
BUFF_TYPE_ELEM_DERATE_RATIO_DEBUFF		= 127			# 元素抗性

#BUFF来源
BUFF_ORIGIN_NONE						= 0				# 无来源或者未知来源
BUFF_ORIGIN_EQUIP						= 1				# 装备
BUFF_ORIGIN_DRUG						= 2				# 药水
BUFF_ORIGIN_SCROLL						= 3				# 卷轴
BUFF_ORIGIN_EVENT						= 4				# 事件
BUFF_ORIGIN_STATE1						= 5				# 状态1
BUFF_ORIGIN_STATE2						= 6				# 状态2
BUFF_ORIGIN_STATE3						= 7				# 状态3
BUFF_ORIGIN_PUBLIC						= 8				# 公共
BUFF_ORIGIN_FINISH						= 9				# 终极
BUFF_ORIGIN_SYSTEM						= 10			# 系统（客户端不提示、不显示）
BUFF_ORIGIN_YXLM						= 11			# 英雄联盟版的失落宝藏副本里NPC出售的装备的buff

# 技能基础分类
BASE_SKILL_TYPE_NONE					= 0				# 非技能（不属于技能类别，如训练技能、普通物理攻击、作为附加spell出现的spell等都是这一类）
BASE_SKILL_TYPE_PASSIVE					= 1				# 被动技能
BASE_SKILL_TYPE_BUFF					= 2				# BUFF技能 (因为BUFF实际也是技能系统中的一员，所以他们都有一个系列的Type分类)
BASE_SKILL_TYPE_PHYSICS_NORMAL			= 3				# 普通物理攻击
BASE_SKILL_TYPE_PHYSICS					= 4				# 物理系技能
BASE_SKILL_TYPE_MAGIC					= 5				# 法术系技能
BASE_SKILL_TYPE_ITEM					= 6				# 道具系技能
BASE_SKILL_TYPE_ACTION					= 7				# 行为技能
BASE_SKILL_TYPE_DISPERSION				= 8				# 驱散系技能
BASE_SKILL_TYPE_ELEM					= 9				# 元素技能
BASE_SKILL_TYPE_POSTURE_PASSIVE			= 10			# 姿态被动技能（仅在某种姿态才有效果的被动技能）



#BUFF系统中断定义
BUFF_INTERRUPT_NONE						= 0				# 无条件 无定义的打断
BUFF_INTERRUPT_GET_HIT					= 1				# 受打击时打断
BUFF_INTERRUPT_REQUEST_CANCEL			= 2				# 玩家手动请求取消BUFF时打断
BUFF_INTERRUPT_ON_DIE					= 3				# 死亡时导致的打断
BUFF_INTERRUPT_ON_CHANGED_SPACE			= 4				# 切换场景时导致打断
BUFF_INTERRUPT_CHANGE_TITLE				= 5				# 更换称号时导致的打断，16:14 2008-7-16，wsf
BUFF_INTERRUPT_RETRACT_VEHICLE			= 6				# 玩家下马中断(骑宠专用)
BUFF_INTERRUPT_RETRACT_PROWL			= 7				# 隐匿中断码
BUFF_INTERRUPT_HORSE					= 8				# 赛马中断码
BUFF_INTERRUPT_INVINCIBLE_EFFECT		= 9				# 所有负面效果的buff都要配置这个中断码， 提供其他技能中断
BUFF_INTERRUPT_POSTURE_CHANGE			= 10			# 姿态切换中断
BUFF_INTERRUPT_VEHICLE_OFF				= 11		# 下骑宠移除骑宠buff
BUFF_INTERRUPT_COMPLETE_VIDEO				= 12            #视频播放完毕移除buff

# entity身上的BUFF状态定义
#BUFF_STATE_NORMAL						= 0x00000000	# 正常状态
BUFF_STATE_HAND							= 0x00000001	# 失效并挂起状态 记时停止
BUFF_STATE_DISABLED						= 0x00000002	# 失效状态 仍然记时

#护盾定义
SHIELD_TYPE_VOID						= 0				# 无类型护盾 可吸收任何类型伤害
SHIELD_TYPE_PHYSICS						= 1				# 物理护盾 吸收物理伤害
SHIELD_TYPE_MAGIC						= 2				# 法力护盾 吸收法力伤害

# 伤害类型，主要用于标识是受到哪种类型的伤害
DAMAGE_TYPE_VOID 						= 0x00			# 无类型伤害(此值一般只用于伤害反弹类型和护盾吸收类型)
DAMAGE_TYPE_PHYSICS_NORMAL				= 0x01			# 普通物理攻击伤害
DAMAGE_TYPE_PHYSICS						= 0x02			# 物理技能伤害
DAMAGE_TYPE_MAGIC						= 0x04			# 法术攻击技能伤害
DAMAGE_TYPE_FLAG_DOUBLE					= 0x08			# 产生了致命一击
DAMAGE_TYPE_FLAG_BUFF					= 0x10			# 伤害来自buff发作
DAMAGE_TYPE_REBOUND						= 0x20			# 反弹伤害
DAMAGE_TYPE_DODGE						= 0x40			# 伤害为0 闪躲了 但仍然属于此次伤害类型中的一种特性
DAMAGE_TYPE_RESIST_HIT					= 0x100			# 招架 但仍然属于此次伤害类型中的一种特性
DAMAGE_TYPE_ELEM_HUO					= 0x200			# 火元素伤害
DAMAGE_TYPE_ELEM_XUAN					= 0x400			# 玄元素伤害
DAMAGE_TYPE_ELEM_LEI					= 0x600			# 雷元素伤害
DAMAGE_TYPE_ELEM_BING					= 0x800			# 冰元素伤害
DAMAGE_TYPE_ELEM						= 0x1000		# 元素伤害类型
DAMAGE_TYPE_DROP						= 0x900		# 掉落伤害

# 受术者条件（哪些人会接受这个法术），
RECEIVER_CONDITION_ENTITY_NONE			= 0x00		# 无条件
RECEIVER_CONDITION_ENTITY_SELF			= 0x01		# 自己
RECEIVER_CONDITION_ENTITY_TEAMMEMBER	= 0x02		# 队员
RECEIVER_CONDITION_ENTITY_MONSTER		= 0x03		# 怪物
RECEIVER_CONDITION_ENTITY_ROLE			= 0x04		# 玩家
RECEIVER_CONDITION_ENTITY_ENEMY			= 0x05		# 敌人
RECEIVER_CONDITION_ENTITY_NOTATTACK		= 0x06		# 不可攻击的
RECEIVER_CONDITION_KIGBAG_ITEM			= 0x07		# 物品
RECEIVER_CONDITION_ENTITY_RANDOMENEMY	= 0x08		# 敌人 （随机区域内攻击另外N个对象）
RECEIVER_CONDITION_ENTITY_SELF_PET		= 0x09		# 己方出战宠物 (相当与自身 无需选择一个目标)
RECEIVER_CONDITION_ENTITY_PET			= 0x10		# 出战宠物 （需要鼠标选择一个宠物）
RECEIVER_CONDITION_ENTITY_NPC			= 0x11		# NPC
RECEIVER_CONDITION_ENTITY_SELF_SLAVE_MONSTER = 0x12 #从属怪物
RECEIVER_CONDITION_ENTITY_OTHER_ROLE	= 0x13		# 其它玩家
RECEIVER_CONDITION_ENTITY_NPC_OR_MONSTER= 0x14		# 怪物或NPC
RECEIVER_CONDITION_ENTITY_NPCOBJECT		= 0x15		# 场景NPC
RECEIVER_CONDITION_ENTITY_NOT_ENEMY_ROLE= 0x16		# 非敌对玩家
RECEIVER_CONDITION_ENTITY_VEHICLE_DART	= 0x17		# 镖车
RECEIVER_CONDITION_ENTITY_ROLE_ONLY		= 0x18		# 仅对玩家有效。历史原因，原来受术者条件为玩家的技能则必定也会对
RECEIVER_CONDITION_ENTITY_ROLE_ENEMY_ONLY = 0x19	# 仅对敌对玩家有效

#技能对象entity类型
SKILL_TARGET_OBJECT_NONE 			= 0x00
SKILL_TARGET_OBJECT_ENTITY 			= 0x01
SKILL_TARGET_OBJECT_POSITION		= 0x02
SKILL_TARGET_OBJECT_ITEM 			= 0x03
SKILL_TARGET_OBJECT_ENTITYS			= 0x04
SKILL_TARGET_OBJECT_ENTITYPACKET	= 0x05
# 施法者条件（哪些人可以施展某个法术）
# 所有参数组合后都是“与”关系，因此，某些状态组合后将会出现问题，
# 如CASTER_CONDITION_FIGHT_STATE与CASTER_CONDITION_FIGHT_NOT_STATE组合将导致法术不可施放
CASTER_CONDITION_ATTACK_ALLOW			= 0x00000001		# 允许使用普通物理攻击
CASTER_CONDITION_SPELL_ALLOW			= 0x00000002		# 允许施法(导致不可以施法的原因很多，如被沉默、昏迷等)
CASTER_CONDITION_BUFF_NO_HAVE			= 0x00000004		# 身上不存在指定类型的buff
CASTER_CONDITION_BUFF_HAVE				= 0x00000008		# 身上必须存在指定类型的buff
CASTER_CONDITION_FIGHT_STATE			= 0x00000010		# 必须处于战斗状态
CASTER_CONDITION_FIGHT_NOT_STATE		= 0x00000020		# 必须处于非战斗状态
CASTER_CONDITION_STATE_DEAD				= 0x00000040		# 已死亡者（这个通常用于通过十字架复活自己的技能）
CASTER_CONDITION_STATE_LIVE				= 0x00000080		# 活着的
CASTER_CONDITION_EMPTY_HAND				= 0x00000100		# 必须空手(两只手都空着)
CASTER_CONDITION_EMPTY_PRIMARY_HAND		= 0x00000200		# 主(拿武器的)手必须为空
CASTER_CONDITION_WEAPON_CONFINE			= 0x00000400		# 必须装备了武器(不管什么武器)
CASTER_CONDITION_POSSESS_ITEM			= 0x00000800		# 必须拥有某物品
CASTER_CONDITION_WEAPON_EQUIP			= 0x00001000		# 必须装备了某类型的武器
CASTER_CONDITION_MOVE_NOT_STATE			= 0x00002000		# 必须没有处于移动状态
CASTER_CONDITION_SHIELD_EQUIP			= 0x00004000		# 必须装备了盾牌
CASTER_CONDITION_STATE_NO_PK			= 0x00008000		# 非PK状态
CASTER_CONDITION_IN_APPOINT_SPACE		= 0x00010000		# 需要指定地图
CASTER_CONDITION_WEAPON_TALISMAN		= 0x00020000		# 需要装备法宝
CASTER_CONDITION_IN_APPOINT_SPACE_NOT_USE = 0x00040000		# 指定地图不能使用
CASTER_CONDITION_NOT_ENEMY_STATE		= 0x00080000		# 是否不为敌对状态
CASTER_CONDITION_POSTURE				= 0x00100000		# 处于某种姿态
CASTER_CONDITION_SPACE_SUPPORT_FLY		= 0x00200000		# 空间支持飞行
CASTER_CONDITION_IS_FLYING_STATUS		= 0x00400000		# 处于飞行状态
CASTER_CONDITION_IS_GROUND_STATUS		= 0x00800000		# 处于地面状态


# 施展技能所需条件对象的类型
SKILL_REQUIRE_TYPE_NONE					= 0x01				# 无类型
SKILL_REQUIRE_TYPE_MANA					= 0x02				# 需要魔法
SKILL_REQUIRE_TYPE_ITEM					= 0x03				# 需要物品
SKILL_REQUIRE_TYPE_MONEY				= 0x04				# 需要金钱

# 效果状态
EFFECT_STATE_SLEEP						= 0x0001			# 昏睡效果
EFFECT_STATE_VERTIGO					= 0x0002			# 眩晕效果
EFFECT_STATE_FIX						= 0x0004			# 定身效果
EFFECT_STATE_HUSH_PHY					= 0x0008			# 物理沉默效果
EFFECT_STATE_HUSH_MAGIC					= 0x0010			# 法术沉默效果
EFFECT_STATE_INVINCIBILITY				= 0x0020			# 无敌效果
EFFECT_STATE_NO_FIGHT					= 0x0040			# 免战效果
EFFECT_STATE_PROWL						= 0x0080			# 潜行效果
EFFECT_STATE_FOLLOW						= 0x0100			# 跟随（玩家处于组队跟随中）
EFFECT_STATE_LEADER						= 0x0200			# 引导（玩家处于组队引导中）
EFFECT_STATE_ALL_NO_FIGHT				= 0x0400			# 全体免战效果(包括任何entity)
EFFECT_STATE_WATCHER					= 0x0800			# GM观察者效果（无法攻击，无法被攻击，无法被查看）
EFFECT_STATE_DEAD_WATCHER				= 0x1000			# 竞技场死亡后隐身（无法攻击，无法被攻击，无法被查看）
EFFECT_STATE_HEGEMONY_BODY				= 0x2000			# 霸体效果，免晕免击退免受击
EFFECT_STATE_BE_HOMING				= 0x4000			# 击退效果,不能主动移动


# --------------------------------------------------------------------
# custom id of skill define( designed by kebiao )
# --------------------------------------------------------------------
# 程序自定义技能ID
SKILL_ID_PHYSICS						= 1				# 普通物理攻击技能ID
SKILL_ID_PLAYER_TELEPORT				= 322361002		# 玩家的默认传送技能
SKILL_ID_CATCH_PRISON					= 780040001		# 抓捕到监狱的技能ID
SKILL_ID_CONJURE_PET 					= 101 			# 召唤宠物
SKILL_ID_SGL_DANCING					= 201			# 单人舞
SKILL_ID_DBL_DANCING					= 202			# 双人舞
#SKILL_ID_TEAM_DANCING					= 203			# 队伍共舞
SKILL_ID_FACE_DRINK						= 11			# 喝水
SKILL_ID_FACE_BYE						= 12			# 挥手
SKILL_ID_FACE_REFUSE					= 13			# 拒绝
SKILL_ID_FACE_DEFY						= 14			# 挑衅
SKILL_ID_FACE_KNEE						= 15			# 跪
SKILL_ID_FACE_TALK						= 16			# 交谈
SKILL_ID_FACE_SMILE						= 17			# 笑
SKILL_ID_FACE_SIT						= 18			# 坐
SKILL_ID_FACE_LIE						= 19			# 躺



SKILL_ID_LIMIT							= 1000			# 程序自用技能ID边界，小于1000为程序自用
# --------------------------------------------------------------------
# ai  define( designed by kebiao )
# --------------------------------------------------------------------
AI_TYPE_GENERIC_FREE					=	1			# 通用AI类型 free状态下
AI_TYPE_GENERIC_ATTACK					=	2			# 通用AI类型 战斗状态下
AI_TYPE_SCHEME							=	3			# 配置AI类型 战斗状态下
AI_TYPE_SPECIAL							=	4			# 特殊AI类型 战斗状态下
AI_TYPE_EVENT							=	5			# 事件AI类型 任何状态下

# ai 事件
AI_EVENT_NONE							=	0			# 味知或其他事件
AI_EVENT_ENEMY_LIST_CHANGED				=	1			# 战斗列表被改变
AI_EVENT_DAMAGE_LIST_CHANGED			=	2			# 伤害列表被改变
AI_EVENT_CURE_LIST_CHANGED				=	3			# 治疗列表被改变
AI_EVENT_FRIEND_LIST_CHANGED			=	30			# 友方列表被改变
AI_EVENT_ATTACKER_ON_REMOVE				=	32			# 当前攻击对象被移出敌人列表时(死亡,不在视野,找不到等)
AI_EVENT_STATE_CHANGED					=	8			# 状态改变
AI_EVENT_SUBSTATE_CHANGED				=	29			# sub状态改变
AI_EVENT_SKILL_HIT						=	9			# 攻击者命中时
AI_EVENT_SKILL_DOUBLEHIT				=	12			# 攻击者致命一击时
AI_EVENT_SKILL_MISS						=	15			# 攻击者未命中
AI_EVENT_SKILL_RESISTHIT				= 	27			# 目标招架自身攻击
AI_EVENT_SKILL_RECEIVE_HIT				=	17			# 自身被命中
AI_EVENT_SKILL_RECEIVE_DOUBLEHIT		=	19			# 自身受到致命一击
AI_EVENT_HP_CHANGED						=	21			# 自身HP改变时
AI_EVENT_MP_CHANGED						=	22			# 自身MP改变时
AI_EVENT_ADD_REMOVE_BUFF				=	23			# 自身BUFF状态有改变
AI_EVENT_SPELL							=	25			# 技能施放时
AI_EVENT_TARGET_DEAD					=	26			# 当前目标死亡时
AI_EVENT_COMMAND						=	28			# 收到AI命令
AI_EVENT_SPELL_ENTERTRAP				=	33			# 有entity进入或者离开自身陷阱事件
AI_EVENT_ENTITY_ON_DESTROY				=	31			# entity被销毁时 (死亡会销毁)
AI_EVENT_ENTITY_ON_DEAD					=   34			# entity自身 死亡
AI_EVENT_SPELL_OVER						=	35			# 技能施放完毕
AI_EVENT_SPELL_INTERRUPTED				=	36			# 技能被打断事件
AI_EVENT_TALK							=	37			# 对话触发AI事件
AI_EVENT_BOOTY_OWNER_CHANGED			=	38			# 战利品拥有者改变了
AI_EVENT_CHANGE_BATTLECAMP				=	39			# 改变怪物阵营事件

# --------------------------------------------------------------------
# about kitbag( from common/L3Define.py；designed by penghuawei )
# --------------------------------------------------------------------
KB_COUNT							= 7		# 最大背包数量（不包括装备栏）( 原名：C_KITBAGMAX )
KB_EQUIP_ID							= 0x00	# 装备栏的位置，这个值必须是在self.KB_COMMON_ID 到 KB_COMMON_ID + KB_COUNT之外的值(原名：C_EQUIP_ORDER )
KB_COMMON_ID						= 0x01	# 背包集合开始位置(原名：C_KITBAG_ORDER )
KB_EXCONE_ID						= 0x02	# 额外包裹1
KB_EXCTWO_ID						= 0x03	# 额外包裹2
KB_EXCTHREE_ID						= 0x04	# 额外包裹3
KB_EXCFOUR_ID						= 0x05	# 额外包裹4
KB_EXCFIVE_ID						= 0x06	# 额外包裹5
KB_EXCSIX_ID						= 0x07	# 额外包裹6
KB_CASKET_ID						= 0x08	# 神机匣包裹位
KB_RACE_ID							= 0x09	# 赛马背包位
KB_MAX_SPACE						= 0xff	# 单个包裹最大空间位
KB_MAX_COLUMN						= 0x06	# 包裹的最大列数
KB_CASKET_SPACE						= 0xfd	# 神机匣水晶摘除起始索引



# --------------------------------------------------------------------
# about bankBags( from common/L3Define.py；designed by wangshufeng )
# --------------------------------------------------------------------
BANK_COUNT							= 7		# 钱庄最大包裹数量
BANK_COMMON_ID						= 0x00	# 钱庄默认的开始包裹位置
BANK_SORT_BY_ID						= 0		# 物品排序类型：类型（id）
BANK_SORT_BY_QUALITY				= 1		# 物品排序类型：品质
BANK_SORT_BY_PRICE					= 2		# 物品排序类型：价格
BANK_SORT_BY_LEVEL					= 3		# 物品排序类型：等级


# --------------------------------------------------------------------
# about quickbar( from common/L3Define.py；designed by huangyongwei )
# --------------------------------------------------------------------
QB_ITEM_NONE						= 0		# 空快捷项
QB_ITEM_SKILL						= 1		# 技能快捷项
QB_ITEM_TACT						= 2		# 角色操作快捷项
QB_ITEM_KITBAG						= 3		# 背包中的物品道具快捷项
QB_ITEM_EQUIP						= 4		# 装备中的道具
QB_ITEM_PET_SKILL					= 5		# 宠物技能快捷项
QB_ITEM_VEHICLE						= 6		# 骑宠物品快捷项

# --------------------------------------------------------------------
QB_IDX_POSTURE_NONE					= 0		# 左下方快捷栏第一页普通状态下的索引
QB_IDX_PAGE2						= 1		# 左下方快捷栏第二页的索引
QB_IDX_PAGE3						= 2		# 左下方快捷栏第三页的索引
QB_IDX_HIDE_BAR						= 3		# 右边的隐藏快捷栏的索引
QB_IDX_POSTURE_DEFENCE				= 5		# 防御姿态对应的快捷栏索引				┌──────────────────┐
QB_IDX_POSTURE_VIOLENT				= 7		# 狂暴姿态对应的快捷栏索引				│索引间隔为2是将一行快捷栏格子的数量 │
QB_IDX_POSTURE_DEVIL_SWORD			= 9		# 魔剑姿态对应的快捷栏索引				│扩展到20个（现在是10个），这样以后  │
QB_IDX_POSTURE_SAGEL_SWORD			= 11	# 圣剑姿态对应的快捷栏索引				│要增加时就不需要作太大改动          │
QB_IDX_POSTURE_SHOT					= 13	# 神射姿态对应的快捷栏索引				└──────────────────┘
QB_IDX_POSTURE_PALADIN				= 15	# 游侠姿态对应的快捷栏索引
QB_IDX_POSTURE_MAGIC				= 17	# 法术姿态对应的快捷栏索引
QB_IDX_POSTURE_CURE					= 19	# 医术姿态对应的快捷栏索引
QB_AUTO_SPELL_INDEX					= 267	# 自动攻击技能快捷栏索引（包括宠物快捷栏、自动战斗快捷栏）


# --------------------------------------------------------------------
# about quest( from common/QuestDefine.py；designed by penghuawei/kebiao )
# --------------------------------------------------------------------
# quest type
QUEST_TYPE_NONE						= 0		# 未定义
QUEST_TYPE_TREASURE					= 1		# 宝藏任务
QUEST_TYPE_POTENTIAL				= 2		# 潜能任务
QUEST_TYPE_PLACARD					= 3		# 公告通缉任务
QUEST_TYPE_CTG						= 4		# 闯天关任务
QUEST_TYPE_DART						= 5		# 运镖任务
QUEST_TYPE_ROB						= 6		# 劫镖任务
QUEST_TYPE_MEMBER_DART				= 7 	# 成员运镖任务
QUEST_TYPE_IMPERIAL_EXAMINATION 	= 8		# 科举考试
QUEST_TYPE_MERCHANT					= 9		# 商会任务
QUEST_TYPE_RUN_MERCHANT				= 10	# 跑商任务
QUEST_TYPE_TONG_FETE				= 11	# 帮会祭祀
QUEST_TYPE_TONG_BUILD				= 12	# 帮会建筑
QUEST_TYPE_TONG_NORMAL				= 13	# 帮会日常


# task objective
QUEST_OBJECTIVE_NONE				= 0		# 不存在的要求( 原名：TASK_OBJECTIVE_NONE )
QUEST_OBJECTIVE_KILL				= 1		# 杀怪( 原名：TASK_OBJECTIVE_KILL )
QUEST_OBJECTIVE_DELIVER				= 2		# 交付物品( 原名：TASK_OBJECTIVE_DELIVER )
QUEST_OBJECTIVE_TIME				= 3		# 时间限制(不能超过此时间)( 原名：TASK_OBJECTIVE_TIME )
QUEST_OBJECTIVE_EVENT_USE_ITEM		= 4		# 使用物品( 原名：TASK_OBJECTIVE_ACTIVE_ITEM )
QUEST_OBJECTIVE_OWN_PET				= 5		# 宠物拥有数量
QUEST_OBJECTIVE_SUBMIT				= 6		# 提交物品
QUEST_OBJECTIVE_EVENT_TRIGGER		= 7		# 事件触发
QUEST_OBJECTIVE_TEAM				= 8		# 组队任务目标
QUEST_OBJECTIVE_LEVEL				= 9		# 等级任务目标
QUEST_OBJECTIVE_QUEST				= 10	# 完成一个其他任务任务目标(专门针对环任务)
QUEST_OBJECTIVE_SUBMIT_PICTURE		= 11 	# 提交画板
QUEST_OBJECTIVE_DELIVER_PET			= 12	# 收集宠物
QUEST_OBJECTIVE_SUBMIT_QUALITY		= 13	# 提交物品（属性：品质）
QUEST_OBJECTIVE_SUBMIT_SLOT			= 14	# 提交物品（属性：孔数）
QUEST_OBJECTIVE_SUBMIT_EFFECT		= 15	# 提交物品（属性：镶嵌）
QUEST_OBJECTIVE_SUBMIT_LEVEL		= 16	# 提交物品（属性：强化）
QUEST_OBJECTIVE_SUBMIT_BINDED		= 17	# 提交物品（属性：绑定）
QUEST_OBJECTIVE_PET_EVENT			= 18 	# 宠物指引任务触发类型
QUEST_OBJECTIVE_DART_KILL			= 19	# 劫镖任务之杀镖车
QUEST_OBJECTIVE_EVOLUTION			= 20	# 怪物进化
QUEST_OBJECTIVE_SUBMIT_YINPIAO		= 21	# 提交物品 (银票)
QUEST_OBJECTIVE_IMPERIAL_EXAMINATION= 22	# 科举考试
QUEST_OBJECTIVE_KILL_WITH_PET		= 23	# 和宠物一起战斗
QUEST_OBJECTIVE_KILL_ROLE_TYPE_MONSTER= 24	# 杀死和角色同样职业的怪物
QUEST_OBJECTIVE_SHOW_KAOGUAN		= 25	# 显示科举考试的当前考官
QUEST_OBJECTIVE_QUEST_NORMAL		= 26	# 完成一个其他任务任务目标(专门针对普通任务)
QUEST_OBJECTIVE_QUESTION			= 27	# 问答形式的任务
QUEST_OBJECTIVE_SKILL_LEARNED		= 28	# 学会了一个技能的任务
QUEST_OBJECTIVE_PET_ACT				= 29	# 出战了一个宠物
QUEST_OBJECTIVE_TALK				= 30	# 队伍任务目标
QUEST_OBJECTIVE_HASBUFF				= 31	# 拥有buff目标
QUEST_OBJECTIVE_DELIVER_QUALITY		= 32	# 交付物品（颜色）
QUEST_OBJECTIVE_SUBMIT_CHANGE_BODY 	= 33	# 提交变身
QUEST_OBJECTIVE_SUBMIT_DANCE	 	= 34	# 提交跳舞
QUEST_OBJECTIVE_POTENTIAL_FINISH	= 35 	# 完成一次潜能任务
QUEST_OBJECTIVE_SUBMIT_LQEQUIP		= 36	# 提交一定品质等级的装备
QUEST_OBJECTIVE_EVENT_USE_SKILL		= 37	# 使用技能
QUEST_OBJECTIVE_EVENT_REVIVE_POS	= 38	# 设置绑定点
QUEST_OBJECTIVE_KILLS = 39	# 杀多种怪
QUEST_OBJECTIVE_ENTER_SPCACE 		= 40	# 到达某一个空间
QUEST_OBJECTIVE_LIVING_SKILL_LEARNED= 41	# 学习生活技能的目标
QUEST_OBJECTIVE_POTENTIAL			= 42    # 潜能任务专用




# quest dialog object for the start or the end.
QUEST_DIALOG_ITEM					= 0		# 物品
QUEST_DIALOG_GAMEOBJECT				= 1		# 游戏对象或NPC

# quest reward type
QUEST_REWARD_NONE					= 0		# 不可使用项
QUEST_REWARD_EXP					= 1		# 奖励经验
QUEST_REWARD_ITEMS					= 2		# 奖励单物品
QUEST_REWARD_CHOOSE_ITEMS			= 3		# 奖励多选一物品
QUEST_REWARD_RANDOM_ITEMS			= 4		# 奖励随机物品
QUEST_REWARD_MONEY					= 5		# 奖励金钱
QUEST_REWARD_TITLE					= 6		# 奖励称号
QUEST_REWARD_SKILL					= 7		# 奖励技能
QUEST_REWARD_POTENTIAL				= 8		# 奖励潜能点
QUEST_REWARD_FIXED_RANDOM_ITEMS		= 9		# 固定随机奖励（必然是随机物品中的一个或没有）
QUEST_REWARD_RELATION_MONEY			= 10	# 关联金钱奖励
QUEST_REWARD_RELATION_EXP			= 11	# 关联经验奖励
QUEST_REWARD_PRESTIGE				= 12	# 声望奖励
QUEST_REWARD_MERCHANT_MONEY			= 13	# 跑商奖励
QUEST_REWARD_TONG_CONTRIBUTE		= 14	# 帮贡奖励
QUEST_REWARD_TONG_BUILDVAL			= 15	# 帮会建设度
QUEST_REWARD_TONG_MONEY				= 16	# 帮会资金
QUEST_REWARD_EXP_FROM_ROLE_LEVEL	= 17	# 根据玩家等级奖励的经验值
QUEST_REWARD_TITLE					= 18	# 称号奖励
QUEST_REWARD_EXP_SECOND_PERCENT		= 19	# 奖励秒经验加成
QUEST_REWARD_TONG_CONTRIBUTE_NORMAL = 20	# 奖励帮贡（默认方式）
QUEST_REWARD_ROLE_LEVEL_MONEY		= 21	# 根据玩家等级奖励的金钱
QUEST_REWARD_PET_EXP				= 22    # 宠物经验奖励
QUEST_REWARD_IE_TITLE				= 23	# 科举称号奖励
QUEST_REWARD_RANDOM_ITEM_FROM_TABLE	= 24	# 读取配置表格物品奖励
QUEST_REWARD_RELATION_PET_EXP		= 25    # 宠物环任务经验奖励
QUEST_REWARD_PET_EXP_FROM_ROLE_LEVEL= 26	# 根据玩家等级奖励给宠物的经验值
QUEST_REWARD_PET_EXP_SECOND_PERCENT	= 27	# 奖励秒经验加成（宠物）
QUEST_REWARD_DEPOSIT				= 28	# 返还押金
QUEST_REWARD_MULTI_EXP				= 29	# 多倍人物经验奖励，例如 8000 x 3
QUEST_REWARD_MULTI_PET_EXP			= 30	# 多倍宠物经验奖励，例如 3000 x 2
QUEST_REWARD_MULTI_MONEY			= 31	# 多倍金钱奖励，例如 5金10银 x 2
QUEST_REWARD_ITEMS_QUALITY			= 32	# 奖励单物品，品质
QUEST_REWARD_SPECIAL_TONG_BUILDVAL	= 33	# 帮会特别建设度奖励
QUEST_REWARD_FUBI_ITEMS				= 34	# 福币奖励
QUEST_REWARD_CHOOSE_ITEMS_AND_BIND	= 35	# 多选一奖励，物品绑定
QUEST_REWARD_TONG_FETE				= 36	# 帮会祭祀装备兑换奖励
QUEST_REWARD_ITEMS_FROM_ROLE_LEVEL	= 37	# 物品奖励（启动物品等级和接任务时玩家等级一样）
QUEST_REWARD_MONEY_TONG_DART		= 38	# 帮会运镖所得金钱
QUEST_REWARD_EXP_TONG_DART 			= 39	# 运镖经验奖励
QUEST_REWARD_TONG_ACTIONVAL			= 40	# 帮会行动力
QUEST_REWARD_CAMP_MORALE			= 41 	# 阵营士气
QUEST_REWARD_CAMP_HONOUR			= 42	# 阵营荣誉
QUEST_REWARD_DAOHENG				= 43	# 道行奖励



QUEST_STATE_NOT_ALLOW				= 0		# 不够条件接任务
QUEST_STATE_NOT_HAVE				= 1		# 还没有接该任务(已经可以接)(无论级别合适与否)
QUEST_STATE_NOT_FINISH				= 2		# 还没有完成目标
QUEST_STATE_FINISH					= 3		# 已完成目标(可以交任务)
QUEST_STATE_COMPLETE				= 4		# 该任务已经做过了
QUEST_STATE_DIRECT_FINISH			= 5		# 该任务可以直接去完成
QUEST_STATE_NOT_HAVE_LEVEL_SUIT		= 6		# 还没有接该任务(已经可以接)(级别合适)。NPC整体任务状态移位标记，不能作为单独任务的判断。
#####################################################################################
# 下面这个任务类型目前没有用于标记任务类型，仅仅用来标记任务图标!!!   			#
# 如果GossipType表中各项位置改动，也需要对此作出改动								#
#####################################################################################
QUEST_STATE_NOT_HAVE_BLUE			= 25	# 蓝色叹号——还没有接该任务(已经可以接)，对应GossipType表中24


QUEST_STYLE_NORMAL					= 0		# 普通任务类型
QUEST_STYLE_RANDOM_GROUP			= 1		# 随机循环组任务类型
QUEST_STYLE_RANDOM					= 2		# 随机循环子任务类型
QUEST_STYLE_DIRECT_FINISH			= 3		# 直接提交任务类型
QUEST_STYLE_FIXED_LOOP				= 4		# 固定环数类型
QUEST_STYLE_LOOP_GROUP				= 5		# 环任务类型
QUEST_STYLE_POTENTIAL				= 6		# 潜能任务样式
QUEST_STYLE_108_STAR				= 7		# 108星任务样式
QUEST_STYLE_TONG_SPACE_COPY			= 8		# 帮会副本组任务
QUEST_STYLE_TONG_FUBEN				= 9     # 帮会副本子任务
QUEST_STYLE_NORMAL1					= 10	# 普通任务类型1（和普通任务类型主要区别：等级差超过10级，依然会显示绿色感叹号的接取任务提示。）
QUEST_STYLE_ABANDOND_ATNPC			= 11	# 只能通过NPC放弃的任务类型

QUEST_REMOVE_FLAG_PLAYER_CHOOSE     = 1		# 玩家主动放弃任务
QUEST_REMOVE_FLAG_NPC_CHOOSE		= 2		# 到指定NPC处放弃

GOSSIP_TYPE_QUEST_STATE_NOT_ALLOW				= 0		# 不够条件接任务对话类型
GOSSIP_TYPE_QUEST_STATE_NOT_HAVE				= 1		# 还没有接该任务(已经可以接)对话类型
GOSSIP_TYPE_QUEST_STATE_NOT_FINISH				= 2		# 还没有完成目标对话类型
GOSSIP_TYPE_QUEST_STATE_FINISH					= 3		# 已完成目标(可以交任务)对话类型
GOSSIP_TYPE_QUEST_STATE_COMPLETE				= 4		# 该任务已经做过了对话类型
GOSSIP_TYPE_QUEST_STATE_DIRECT_FINISH			= 5		# 该任务可以直接去完成对话类型
GOSSIP_TYPE_TRADE								= 6		# 交易对话类型
GOSSIP_TYPE_NORMAL_TALKING						= 7		# 普通对话类型
GOSSIP_TYPE_SKILL_LEARN							= 8		# 技能学习对话类型


# --------------------------------------------------------------------
# about chat
# all are modified :
#	① reclassfied all channels
#	② one channel for one thing
#	by hyw--2009.08.13
# --------------------------------------------------------------------
# 角色发言频道（注意：频道前缀一定是“CHAT_CHANNEL_”）
CHAT_CHANNEL_NEAR					= 1				# 附近频道
CHAT_CHANNEL_LOCAL					= 2				# 本地频道
CHAT_CHANNEL_TEAM					= 3				# 队伍频道
CHAT_CHANNEL_FAMILY					= 4				# 家族频道
CHAT_CHANNEL_TONG					= 5				# 帮会频道
CHAT_CHANNEL_WHISPER				= 6				# 私聊频道
CHAT_CHANNEL_WORLD					= 7				# 世界频道(需要游戏币)
CHAT_CHANNEL_RUMOR					= 8				# 谣言频道
CHAT_CHANNEL_WELKIN_YELL			= 9				# 天音（普通文字广播，需要金币）
CHAT_CHANNEL_TUNNEL_YELL			= 10			# 地音（可带表情广播，需要银元）

# GM/公告频道
CHAT_CHANNEL_SYSBROADCAST			= 11			# 广播(不许玩家发言)

# NPC 发言频道
CHAT_CHANNEL_NPC_SPEAK				= 12			# NPC（包括：NPC 附近（名称前缀为“N”）、NPC 密语（名称前缀为“M”）、NPC 世界（名称前缀为“W”））
CHAT_CHANNEL_NPC_TALK				= 13			# NPC 对话频道

# 系统提示频道
CHAT_CHANNEL_SYSTEM					= 14			# 系统频道（显示由服务器产生的各种活动、获得物品/强化/镶嵌等产生）
CHAT_CHANNEL_COMBAT					= 15			# 战斗频道（显示战斗信息）
CHAT_CHANNEL_PERSONAL				= 16			# 个人频道（个人频道显示玩家在获得经验、潜能、金钱、物品、元宝的信息）
CHAT_CHANNEL_MESSAGE				= 17			# 消息频道（显示角色的操作产生的错误信息或提示信息）
CHAT_CHANNEL_SC_HINT				= 18			# 在屏幕中间提示的频道
CHAT_CHANNEL_MSGBOX					= 19			# 以 MessageBox 提示的频道

# 独立窗口聊天
CHAT_CHANNEL_PLAYMATE				= 20			# 玩伴聊天

# 独立窗口聊天
CHAT_CHANNEL_CAMP					= 21			# 阵营频道

# -------------------------------------------
# 禁言原因
CHAT_FORBID_BY_GM					= 1		# 被 GM 禁言
CHAT_FORBID_REPEAT					= 2		# 因重复发言到一定次数而被禁言
CHAT_FORBID_JAIL					= 3		# 因入狱而被禁言
CHAT_FORBID_GUANZHAN				= 4		# 身上有观战BUFF

# --------------------------------------------------------------------
# about friend( from common/FriendType.py；designed by panguankong )
# --------------------------------------------------------------------
FRIEND_KINDLY_GROUPID				= 1		# 好友组(原名：FRIEND_GROUPID )
FRIEND_BLACKLIST_GROUPID			= 0		# 黑名单组( 原名：BLACKLIST_GROUPID )


# --------------------------------------------------------------------
# about relation( from L3Define.py )
# --------------------------------------------------------------------
RELATION_NONE						= 0 	# 无任何关系，一般用于默认值
RELATION_FRIEND						= 1 	# 好友关系( 原名：C_RELATION_FRIEND )
RELATION_NEUTRALLY					= 2 	# 中立关系( 原名：C_RELATION_NEUTRALLY )
RELATION_ANTAGONIZE 				= 3 	# 敌对关系( 原名：C_RELATION_ANTAGONIZE )
RELATION_NOFIGHT					= 4		# 免战关系

# --------------------------------------------------------------------
# about trade( from L3Define.py )
# --------------------------------------------------------------------
# 交易状态, 标识当前处于哪种交易状态
TRADE_NONE							= 0		# 无交易
TRADE_CHAPMAN						= 1		# 与商人NPC交易(买入/卖出)
TRADE_SWAP							= 2		# 玩家之间金钱/物品交换
TRADE_INVENTORY						= 4		# 仓库操作
TRADE_CASKET						= 8		# 神机匣操作
TRADE_PRODUCE						= 16	# 装备打造
TRADE_TISHOU						= 32	# 寄售物品
TRADE_MAX							= 3		# 有哪些交易操作，数量是除了 TRADE_NONE 外TRADE_*的总数

# 与玩家交易子状态
TRADE_SWAP_DEFAULT					= 0		# 交易默认状态
TRADE_SWAP_INVITE					= 1		# 交易邀请状态
TRADE_SWAP_WAITING					= 2		# 交易等待状态(状态持续15秒后如果没反应则交易取消)
TRADE_SWAP_BEING					= 3		# 物品交易开始状态
TRADE_SWAP_LOCK						= 4		# 物品锁定状态
TRADE_SWAP_PET_INVITE				= 5		# 宠物交易邀请状态
TRADE_SWAP_PET_WAITING				= 6		# 宠物交易等待状态
TRADE_SWAP_PET_BEING				= 7		# 宠物交易开始状态
TRADE_SWAP_PET_LOCK					= 8		# 宠物交易锁定状态
TRADE_SWAP_SURE						= 9		# 交易确定状态
TRADE_SWAP_LOCKAGAIN				= 10	# 双方再次锁定状态

TRADE_MAX_GOODS_NUM					= 100	# 玩家与商人交易最多可以买的商品数量上限

TRADE_REQUIRE_AMOUNT_TIME			= 30	# 请求商品数量间隔时间

# --------------------------------------------------------------------
# about pk
# --------------------------------------------------------------------
PK_STATE_PROTECT					= 0		# 保护状态
PK_STATE_PEACE						= 1		# 白名状态
PK_STATE_ATTACK						= 2		# 褐名(攻击)状态(120秒内没有再次触发变为非攻击状态)
PK_STATE_REDNAME					= 3		# 红名状态
PK_STATE_BLUENAME					= 4		# 蓝名状态
PK_STATE_ORANGENAME					= 5		# 橙名状态（也就是小红名状态）


#pk的最低等级
PK_ALLOW_LEVEL_MIN					= 31	# 31级才可以pk

#pk模式
PK_CONTROL_PROTECT_PEACE			= 0		# 和平模式
PK_CONTROL_PROTECT_TEAMMATE			= 1		# 组队模式
PK_CONTROL_PROTECT_KIN				= 2		# 家族模式
PK_CONTROL_PROTECT_TONG				= 3		# 帮会模式
PK_CONTROL_PROTECT_NONE				= 4		# 全体模式
PK_CONTROL_PROTECT_RIGHTFUL			= 5		# 善恶模式


#pk的最低等级
PK_CATCH_VALUE						= 60	# pk值在这个点数及以上抓捕入狱

# --------------------------------------------------------------------
# about equip
# --------------------------------------------------------------------
EQUIP_ABRASION_NORMALATTACK			= 1		# 武器损耗普通攻击类型
EQUIP_ABRASION_SPELLATTACK			= 2		# 武器损耗技能攻击类型


EQUIP_REPAIR_NORMAL					= 1		# 普通修理
EQUIP_REPAIR_SPECIAL				= 2		# 特殊修理
EQUIP_REPAIR_ITEM				= 3		# 通过物品修理


#物品进入背包返回状态
KITBAG_CAN_HOLD						= 0		# 背包可以容纳进来的物品
KITBAG_NO_MORE_SPACE				= 1		# 背包空间不够
KITBAG_ITEM_COUNT_LIMIT				= 2		# 玩家拥有这类物品超出拥有数量
KITBAG_ADD_ITEM_BY_STACK_SUCCESS	= 3		# 通过叠加物品来添加物品
KITBAG_ADD_ITEM_SUCCESS				= 4		# 添加物品成功
KITBAG_ADD_ITEM_FAILURE				= 5		# 添加物品失败
KITBAG_STACK_ITEM_SUCCESS			= 6		# 叠加成功
KITBAG_STACK_ITEM_NO_SAME_ITEM		= 7		# 叠加物品时没有相同的物品
KITBAG_STACK_ITEM_NO_MORE			= 8		# 叠加物品丝时包裹物品不可以叠加
KITBAG_STACK_DSTITEM_NO_MORE		= 9		# 两个物品叠加时目标物品不可以叠加
KITBAG_STACK_ITEM_ID_DIFF			= 10	# 两个物品叠加时两物品同类
KITBAG_STACK_ITEM_SUCC_1			= 11	# 两个物品叠加成功但源物品有剩余
KITBAG_STACK_ITEM_SUCC_2			= 12	# 两个物品叠加成功且没有剩余


# ---------------------------------------------------------------------
# 默认金钱、背包编号定义
# --------------------------------------------------------------------
ITEMID_MONEY			= 60101001	# 金钱		money
ITEMID_KITBAG_EQUIP		= 70101006	# 装备栏    kitbag_equip
ITEMID_KITBAG_NORMAL	= 70101007	# 默认背包  kitbag_normal
ITEMID_KITBAG_CASKET	= 70101008	# 神机匣    kitbag_casket


# ---------------------------------------------------------------------
# 道具商城，物品类型标志位定义( wsf )
# ---------------------------------------------------------------------
SPECIALSHOP_OTHER_GOODS				= 0x000		# 其他商品
SPECIALSHOP_HOT_GOODS				= 0x001		# 热销类商品标记
SPECIALSHOP_RECOMMEND_GOODS		= 0x002		# 推荐商品标记(现在的热销商品)
SPECIALSHOP_ESPECIAL_GOODS			= 0x004		# 天材地宝类商品标记
SPECIALSHOP_CURE_GOODS				= 0x008		# 恢复类商品标记
SPECIALSHOP_REBUILD_GOODS			= 0x010		# 打造类商品标记
SPECIALSHOP_VEHICLE_GOODS			= 0x020		# 坐骑类商品标记
SPECIALSHOP_PET_GOODS				= 0x040		# 宠物类商品标记
SPECIALSHOP_FASHION_GOODS			= 0x080		# 时装类商品标记
SPECIALSHOP_ENHANCE_GOODS			= 0x100		# 强化材料类商品标记
SPECIALSHOP_CRYSTAL_GOODS			= 0x200		# 水晶类商品标记
SPECIALSHOP_TALISMAN_GOODS			= 0x400		# 法宝类商品标记

SPECIALSHOP_SUBTYPE_MOS_CRYSTAL	= 0			# 镶嵌水晶子类
SPECIALSHOP_SUBTYPE_VAL_GOODS		= 1			# 奇珍异宝子类
SPECIALSHOP_SUBTYPE_EXP_GOODS		= 2			# 经验灵丹子类
SPECIALSHOP_SUBTYPE_REST_GOODS	= 3			# 恢复药品子类
SPECIALSHOP_SUBTYPE_LAND_VEHICLE	= 4			# 陆行坐骑子类
SPECIALSHOP_SUBTYPE_SKY_VEHICLE		= 5			# 飞行坐骑子类
SPECIALSHOP_SUBTYPE_PET_BOOK		= 6			# 宠物书籍子类
SPECIALSHOP_SUBTYPE_PET_PROPS		= 7			# 宠物道具子类
SPECIALSHOP_SUBTYPE_PET_EGG			= 8			# 宠物蛋子类
SPECIALSHOP_SUBTYPE_MALE_FASHION	= 9			# 男性时装子类
SPECIALSHOP_SUBTYPE_FEMALE_FASHION = 10			# 女性时装子类

SPECIALSHOP_GOODS_TYPES = SPECIALSHOP_HOT_GOODS | SPECIALSHOP_RECOMMEND_GOODS | SPECIALSHOP_ESPECIAL_GOODS | SPECIALSHOP_CURE_GOODS | SPECIALSHOP_VEHICLE_GOODS | SPECIALSHOP_PET_GOODS | SPECIALSHOP_FASHION_GOODS


SPECIALSHOP_MONEY_TYPE_GOLD			= 0			# 购买商城道具的货币类型：金元宝
SPECIALSHOP_MONEY_TYPE_SILVER		= 1			# 购买商城道具的货币类型：银元宝

# 请求物品时的商城状态（hyw--2009.03.27）
SPECIALSHOP_REQ_SUCCESS				= 1			# 请求成功
SPECIALSHOP_REQ_FAIL_CLOSED			= 2			# 商城没开
SPECIALSHOP_REQ_FAIL_NOT_EXIST		= 3			# 商品不存在
SPECIALSHOP_REQ_FAIL_LOCKED			= 4			# 商品不可购买


# --------------------------------------------------------------------
# 神机匣
# --------------------------------------------------------------------
# 神机匣功能
CASKET_FUC_STUFFCOMPOSE				= 1		# 材料合成
CASKET_FUC_EQUIPSTILETTO			= 2		# 装备打孔
CASKET_FUC_EQUIPSPLIT				= 3		# 装备拆分
CASKET_FUC_EQUIPSTUDDED				= 4		# 装备镶嵌
CASKET_FUC_EQUIPINTENSIFY			= 5		# 装备强化
CASKET_FUC_EQUIPREBUILD				= 6		# 装备改造
CASKET_FUC_EQUIPBIND				= 7		# 装备绑定
CASKET_FUC_EQUIPPERSONALITY			= 8 	# 装备个性化
CASKET_FUC_EQUIPWASHATTR			= 9		# 装备洗属性
CASKET_FUC_SPECIALCOMPOSE			= 10	# 特殊合成
CASKET_FUC_TALISMANFIX				= 11	# 法宝修复
CASKET_FUC_TALISMAN_SPLIT			= 12	# 法宝分解
CASKET_FUC_TALISMAN_INS				= 13	# 法宝强化
CASKET_FUC_REMOVECRYSTAL			= 14	# 水晶摘除
CASKET_FUC_CHANGEPROPERTY			= 15	# 洗前缀
CASKET_FUC_IMPROVEQUALITY			= 16	#绿装升品

# 装备强化相关
EQUIP_INTENSIFY_MAX_LEVEL			= 9		# 强化的最高等级

# --------------------------------------------------------------------
# 复活方式
# --------------------------------------------------------------------
REVIVE_ON_CITY						= 1		# 回城复活
REVIVE_ON_ORIGIN					= 2		# 原地复活
REVIVE_ON_TOMB						= 3		# 墓地复活
REVIVE_BY_ITEM						= 4		# 通过某个物品复活
REVIVE_BY_BUFF						= 5		# 通过某个BUFF复活
REVIVE_ON_SPACECOPY					= 6		# 在副本入口复活
REVIVE_PRE_SPACE					= 7		# 复活到上一个地图

# --------------------------------------------------------------------
# about mail
# --------------------------------------------------------------------
# 信件发送者类型
MAIL_SENDER_TYPE_PLAYER				= 1		# 玩家寄信
MAIL_SENDER_TYPE_NPC				= 2		# npc寄信
MAIL_SENDER_TYPE_GM					= 3		# GM寄信
MAIL_SENDER_TYPE_RETURN				= 4		# 退信

# 信件类型
MAIL_TYPE_QUICK						= 1		# 快递
MAIL_TYPE_NORMAL					= 2		# 普通邮件

# --------------------------------------------------------------------
# about team pickup state
# --------------------------------------------------------------------
TEAM_PICKUP_STATE_FREE				= 0		# 自由拾取
TEAM_PICKUP_STATE_ORDER				= 1		# 轮流拾取
TEAM_PICKUP_STATE_SPECIFY			= 2		# 队长指定


# --------------------------------------------------------------------
# about spaceType( kebiao )
# --------------------------------------------------------------------
SPACE_TYPE_NORMAL					= 0		# 普通地图
SPACE_TYPE_CITY_WAR					= 2		# 城市争夺战场
SPACE_TYPE_TONG_ABA					= 3		# 帮会擂台赛战场
SPACE_TYPE_TONG_TERRITORY			= 4		# 帮会领地
SPACE_TYPE_SUN_BATHING				= 5		# 日光浴海滩 -spf
SPACE_TYPE_TIAN_GUAN				= 6		# 天关副本
SPACE_TYPE_RACE_HORSE				= 7		# 赛马副本
SPACE_TYPE_POTENTIAL				= 8		# 潜能副本
SPACE_TYPE_WU_DAO					= 9     # 武道大会
SPACE_TYPE_SHEN_GUI_MI_JING			= 10    # 神鬼秘境
SPACE_TYPE_WU_YAO_QIAN_SHAO			= 11	# 巫妖前哨
SPACE_TYPE_WU_YAO_WANG_BAO_ZANG		= 12	# 巫妖王宝藏
SPACE_TYPE_SHUIJING					= 13	# 水晶副本
SPACE_TYPE_HUNDUN					= 14	# 混沌入侵
SPACE_TYPE_TEAM_COMPETITION			= 15	# 组队竞赛
SPACE_TYPE_DRAGON					= 16	# 大头暴龙活动
SPACE_TYPE_PROTECT_TONG				= 17	# 保护帮派
SPACE_TYPE_POTENTIAL_MELEE			= 18	# 潜能乱斗副本
SPACE_TYPE_EXP_MELEE				= 19	# 经验乱斗副本
SPACE_TYPE_PIG						= 20	# 嘟嘟猪副本
SPACE_TYPE_PRISON					= 21	# 监狱
SPACE_TYPE_YAYU						= 22
SPACE_TYPE_XIE_LONG_DONG_XUE		= 23	# 邪龙洞穴
SPACE_TYPE_FJSG						= 24	# 封剑神宫
SPACE_TYPE_TONG_COMPETITION			= 25	# 帮会竞技
SPACE_TYPE_ROLE_COMPETITION			= 26	# 个人竞技
SPACE_TYPE_SHE_HUN_MI_ZHEN			= 27	# 摄魂迷阵
SPACE_TYPE_TEACH_KILL_MONSTER		= 28	# 师徒副本
SPACE_TYPE_KUAFU_REMAINS 			= 29	# 夸父神殿
SPACE_TYPE_RABBIT_RUN				= 30	# 小兔快跑
SPACE_TYPE_BEFORE_NIRVANA			= 31	# 10级副本
SPACE_TYPE_CHALLENGE				= 32	# 挑战副本
SPACE_TYPE_TEAM_CHALLENGE			= 33	# 组队擂台
SPACE_TYPE_PLOT_LV40				= 34	# 40级剧情副本
SPACE_TYPE_PLOT_LV60				= 35	# 60级剧情副本
SPACE_TYPE_TOWER_DEFENSE			= 36	# 搭防副本
SPACE_TYPE_YXLM						= 37	# 英雄联盟副本

# --------------------------------------------------------------------
# about family( kebiao )
# --------------------------------------------------------------------
FAMILY_AFFICHE_LENGTH_MAX			= 200	# 家族公告字数最大
FAMILY_MEMBER_COUNT_MAX				= 15	# 家族人数上限
FAMILY_MEMBER_LEVEL_LOW				= 10	# 加入家族的成员最低级别
FAMILY_CREATE_MONEY					= 100000# 创建家族所需的费用
FAMILY_CREATE_LEVEL					= 30	# 创建家族所需等级

#家族权利
FAMILY_GRADE_MEMBER					= 1		# 家族成员
FAMILY_GRADE_SHAIKH					= 101	# 家族族长
FAMILY_GRADE_SHAIKH_SUBALTERN		= 100	# 家族副族长



# --------------------------------------------------------------------
# about tong( kebiao )
# --------------------------------------------------------------------
TONG_AFFICHE_LENGTH_MAX				= 200	# 帮会公告字数最大
TONG_MEMBER_SCHOLIUM_LENGTH_MAX		= 7		# 帮会成员批注最大字数
TONG_CREATE_MONEY					= 300000# 创建帮会所需的费用
TONG_CREATE_LEVEL					= 30	# 创建帮会所需等级
TONG_LEAGUE_MAX_COUNT				= 2		# 帮会同盟的最大个数
TONG_AMOUNT_MAX						= 100	# 服务器能创建的帮会数量
TONG_AD_LENGTH_MAX 					= 40    # 帮会宣传字数最大

#帮会权利 喽啰——精英——小头目——商人——堂主——护法——副帮主——帮主
TONG_GRADE_MEMBER					= 0x00000001		# 喽啰
TONG_GRADE_ELITE					= 0x00000002		# 精英
TONG_GRADE_RINGLEADER				= 0x00000004		# 小头目
TONG_GRADE_DEALER					= 0x00000008		# 商人
TONG_GRADE_TONG						= 0x00000010		# 堂主
TONG_GRADE_BODYGUARD				= 0x00000020		# 护法
TONG_GRADE_CHIEF_SUBALTERN			= 0x00000040		# 副帮主
TONG_GRADE_CHIEF					= 0x00000080		# 帮主

TONG_GRADES							= TONG_GRADE_MEMBER  | TONG_GRADE_ELITE | TONG_GRADE_RINGLEADER | TONG_GRADE_DEALER | TONG_GRADE_TONG | TONG_GRADE_BODYGUARD | TONG_GRADE_CHIEF | TONG_GRADE_CHIEF_SUBALTERN

TONG_DUTY_GRADE_CONSCRIBE			= 0x00000001 # 招人权限
TONG_DUTY_GRADE_AFFICHE				= 0x00000002 # 发布公告
TONG_DUTY_GRADE_KICK_MEMBER			= 0x00000004 # 开除成员
TONG_DUTY_GRADE_SET_GRADE			= 0x00000008 # 提升降职
TONG_DUTY_GRADE_PROCLAIM_WAR		= 0x00000010 # 宣战权
TONG_DUTY_GRADE_BUILDING			= 0x00000020 # 建筑修建权
TONG_DUTY_GRADE_PET_SELECT			= 0x00000040 # 神兽选择权
TONG_DUTY_GRADE_CREATE_ITEM			= 0x00000080 # 物品生产权
TONG_DUTY_GRADE_CREATE_SKILL		= 0x00000100 # 技能研究权
TONG_DUTY_GRADE_GROUP_ACTION		= 0x00000200 # 集体活动权
TONG_DUTY_GRADE_CHAT				= 0x00000400 # 听取帮会聊天
TONG_DUTY_GRADE_WAREHOUSE_SAVE		= 0x00000800 # 帮会仓库存储权限
TONG_DUTY_GRADE_WAREHOUSE_TAKE_OUT	= 0x00001000 # 帮会仓库取出权限

# 帮主可通过赋权分配的所有权利
TONG_GRADE_CAN_SET_CONSCRIBE 		= TONG_GRADE_CHIEF | TONG_GRADE_CHIEF_SUBALTERN | TONG_GRADE_BODYGUARD | TONG_GRADE_TONG #招收家族	副帮主、护法、堂主
TONG_GRADE_CAN_SET_AFFICHE 			= TONG_GRADE_CHIEF | TONG_GRADE_CHIEF_SUBALTERN | TONG_GRADE_BODYGUARD #发布公告	副帮主、护法
TONG_GRADE_CAN_SET_GRADE 			= TONG_GRADE_CHIEF | TONG_GRADE_CHIEF_SUBALTERN | TONG_GRADE_BODYGUARD | TONG_GRADE_TONG #提升降职	副帮主、护法、堂主
TONG_GRADE_PROCLAIM_WAR				= TONG_GRADE_CHIEF | TONG_GRADE_CHIEF_SUBALTERN #宣战权	副帮主
TONG_GRADE_BUILDING					= TONG_GRADE_CHIEF | TONG_GRADE_CHIEF_SUBALTERN | TONG_GRADE_BODYGUARD | TONG_GRADE_TONG #建筑修建权	副帮主、护法、堂主
TONG_GRADE_TONG_PET_SELECT			= TONG_GRADE_CHIEF | TONG_GRADE_CHIEF_SUBALTERN | TONG_GRADE_BODYGUARD | TONG_GRADE_TONG #神兽选择权	副帮主、护法、堂主
TONG_GRADE_CREATE_ITEM				= TONG_GRADE_CHIEF | TONG_GRADE_CHIEF_SUBALTERN | TONG_GRADE_BODYGUARD | TONG_GRADE_TONG #物品生产权	副帮主、护法、堂主
TONG_GRADE_CREATE_SKILL				= TONG_GRADE_CHIEF | TONG_GRADE_CHIEF_SUBALTERN | TONG_GRADE_BODYGUARD | TONG_GRADE_TONG #技能研究权	副帮主、护法、堂主
TONG_GRADE_GROUP_ACTION				= TONG_GRADE_CHIEF | TONG_GRADE_CHIEF_SUBALTERN #集体活动权	副帮主
TONG_GRADE_CAN_CHAT					= TONG_GRADE_CHIEF | TONG_GRADE_CHIEF_SUBALTERN | TONG_GRADE_MEMBER | TONG_GRADE_ELITE | TONG_GRADE_RINGLEADER | TONG_GRADE_DEALER | TONG_GRADE_TONG | TONG_GRADE_BODYGUARD
TONG_GRADE_CAN_WAREHOUSE_SAVE		= TONG_GRADE_CHIEF | TONG_GRADE_CHIEF_SUBALTERN | TONG_GRADE_MEMBER | TONG_GRADE_ELITE | TONG_GRADE_RINGLEADER | TONG_GRADE_DEALER | TONG_GRADE_TONG | TONG_GRADE_BODYGUARD
TONG_GRADE_CAN_WAREHOUSE_TAKE_OUT	= TONG_GRADE_CHIEF | TONG_GRADE_CHIEF_SUBALTERN | TONG_GRADE_MEMBER | TONG_GRADE_ELITE | TONG_GRADE_RINGLEADER | TONG_GRADE_DEALER | TONG_GRADE_TONG | TONG_GRADE_BODYGUARD

TONG_GRADE_CAN_SET_LEAGUE			= TONG_GRADE_CHIEF | TONG_GRADE_CHIEF_SUBALTERN #同盟权 副帮主

# 帮会建筑类别
TONG_BUILDING_TYPE_YSDT	= 0x00000001	# 议事大厅
TONG_BUILDING_TYPE_JK	= 0x00000002	# 金库
TONG_BUILDING_TYPE_SSD	= 0x00000004	# 神兽殿
TONG_BUILDING_TYPE_CK	= 0x00000008	# 仓库
TONG_BUILDING_TYPE_TJP	= 0x00000010	# 铁匠铺
TONG_BUILDING_TYPE_SD	= 0x00000020	# 商店
TONG_BUILDING_TYPE_YJY	= 0x00000040	# 研究院

# 帮会神兽类别
TONG_SHENSHOU_TYPE_1	= 1				# 青翼龙王
TONG_SHENSHOU_TYPE_2	= 2				# 白毛虎神
TONG_SHENSHOU_TYPE_3	= 3				# 玄武圣君
TONG_SHENSHOU_TYPE_4	= 4				# 炽焰朱雀


# 帮会仓库操作定义
TONG_STORAGE_OPERATION_ADD		= 1			# 存入物品
TONG_STORAGE_OPERATION_MINUS	= 0			# 取出物品

# 帮会城市战争
TONG_CW_FLAG_MONSTER			= 1			# 普通小怪
TONG_CW_FLAG_LP					= 2			# 龙炮
TONG_CW_FLAG_TOWER				= 3			# 塔楼
TONG_CW_FLAG_XJ					= 4			# 守方BOSS


TONG_SKILL_ALL			= 0			# 所有帮会技能
TONG_SKILL_ROLE		= 1			# 帮会角色技能类型
TONG_SKILL_PET			= 2			# 帮会宠物技能类型


# --------------------------------------------------------------------
# about prestige( wangshufeng )
# --------------------------------------------------------------------
PRESTIGE_ENEMY				=			1	# 仇敌
PRESTIGE_STRANGE			=			2	# 冷淡
PRESTIGE_NEUTRAL			=			3	# 中立
PRESTIGE_FRIENDLY			=			4	# 友善
PRESTIGE_RESPECT			=			5	# 尊重
PRESTIGE_ADMIRE				=			6	# 崇敬
PRESTIGE_ADORE				=			7	# 崇拜


# --------------------------------------------------------------------
# about title( wangshufeng )
# --------------------------------------------------------------------
TITLE_CONFIRM_TYPE			= 1			# 一般类型标记
TITLE_COUPLE_TYPE			= 2			# 夫妻类型标记
TITLE_TONG_TYPE				= 3			# 帮会类型标记
TITLE_MERCHANT_TYPE			= 4			# 跑商类型标记
TITLE_IE_TYPE				= 5			# 科举类型标记
TITLE_TEACH_PRENTICE_TYPE	= 6			# 徒弟类型标记
TITLE_TEACH_MASTER_TYPE		= 7			# 师父称号类型标记

TITLE_COUPLE_MALE_ID		= 4			# 称号“%s的老公”的id
TITLE_COUPLE_FEMALE_ID		= 5			# 称号“%s的老婆”的id
TITLE_TEACH_PRENTICE_ID		= 3			# 称号“%s的徒弟”的id
TITLE_ALLY_ID					= 91		# 结拜称号id

TITLE_NONE						= 0			#无称号
TITLE_NOVICIATE_LANLUHU_X		= 8		#兴隆镖局拦路虎
TITLE_NOVICIATE_CHANGZISHOU_X	= 9		#兴隆镖局趟子手
TITLE_NOVICIATE_NEW_DART_X		= 10	#兴隆镖局新手镖师
TITLE_NOVICIATE_WUSHI_X			= 11	#兴隆镖局武师
TITLE_NOVICIATE_DART_HEAD_X		= 12	#兴隆镖局镖头
TITLE_NOVICIATE_DART_FU_X		= 13	#兴隆镖局副总镖头
TITLE_NOVICIATE_DART_KING_X		= 14	#兴隆镖局镖王
TITLE_NOVICIATE_CHANGZISHOU_C	= 15	#昌平镖局趟子手
TITLE_NOVICIATE_NEW_DART_C		= 16	#昌平镖局新手镖师
TITLE_NOVICIATE_WUSHI_C			= 17	#昌平镖局武师
TITLE_NOVICIATE_DART_HEAD_C		= 18	#昌平镖局镖头
TITLE_NOVICIATE_DART_FU_C		= 19	#昌平镖局副总镖头
TITLE_NOVICIATE_DART_KING_C		= 20	#昌平镖局镖王
TITLE_NOVICIATE_LANLUHU_C		= 21	#昌平镖局拦路虎

FACTION_XINGLONG				= 37	# 兴隆镖局势力id
FACTION_CHANGPING				= 38	# 昌平镖局势力id

# --------------------------------------------------------------------
# about RoleGem( wangshufeng )
# --------------------------------------------------------------------
GEM_ROLE_ACTIVE_FLAG		= 0x01		# 角色经验石激活标志位
GEM_PET_ACTIVE_FLAG			= 0x02		# 宠物经验石激活标志位
GEM_ROLE_COMMON_INDEX		= 0			# 玩家经验石的初始index
GEM_PET_COMMON_INDEX		= 50		# 玩家宠物经验石的初始index
GEM_WORK_NORMAL				= 1			# 普通代练类型
GEM_WORK_HARD				= 2			# 刻苦代练类型

# ---------------------------------------------------------------------
# 帮会擂台赛相关定义
# ---------------------------------------------------------------------
ABATTOIR_GAME_NONE			= 0	# 无比赛
ABATTOIR_EIGHTHFINAL		= 1	# 八分之一决赛
ABATTOIR_QUARTERFINAL		= 2	# 四分之一决赛
ABATTOIR_SEMIFINAL			= 3	# 半决赛
ABATTOIR_FINAL				= 4	# 决赛

# ---------------------------------------------------------------------
# 帮会夺城战级别定义，比赛级别是递增关系，可以比较大小
# ---------------------------------------------------------------------
CITY_WAR_LEVEL_NONE	= 0				# 无比赛
CITY_WAR_LEVEL_QUARTERFINAL = 1		# 4分之一决赛
CITY_WAR_LEVEL_SEMIFINAL = 2		# 半决赛
CITY_WAR_LEVEL_FINAL = 3			# 决赛

# ---------------------------------------------------------------------
# 帮会仓库包裹位定义( wsf )
# ---------------------------------------------------------------------
TONG_STORAGE_ONE			= 0	# 包裹1
TONG_STORAGE_TWO			= 1	# 包裹2
TONG_STORAGE_THREE			= 2	# 包裹3
TONG_STORAGE_FOUR			= 3	# 包裹4
TONG_STORAGE_FIVE			= 4	# 包裹5
TONG_STORAGE_SIX			= 5	# 包裹6

TONG_STORAGE_COUNT			= 6	# 仓库包裹最大数量


# ---------------------------------------------------------------------
# 扩充仓库时的一些常量
# ---------------------------------------------------------------------

ID_OF_ITEM_OPEN_BAG			= 110103020


# ---------------------------------------------------------------------
# 活动类型定义( wsf,15:37 2009-1-14 )
# ---------------------------------------------------------------------
ACTIVITY_MONSTER_ATTACK			= 1			# 怪物攻城
ACTIVITY_HORSE_RACE				= 2			# 赛马
ACTIVITY_EXAMINATION_XIANGSHI	= 3			# 科举：乡试
ACTIVITY_EXAMINATION_HUISHI		= 4			# 科举：会试
ACTIVITY_EXAMINATION_DIANSHI	= 5			# 科举：殿试
ACTIVITY_TIANGUAN				= 6			# 天关
ACTIVITY_DARK_TRADER			= 7			# 黑市商人
ACTIVITY_WUDAO					= 8			# 武道大会
ACTIVITY_LUCKY_BOX				= 9			# 天降宝盒
ACTIVITY_BODY_CHANGE_GAME		= 10		# 变身大赛


# ---------------------------------------------------------------------
# 改变金钱的方式( zds,15:19 2009-2-7 )
# ---------------------------------------------------------------------
CHANGE_MONEY_INITIAL				= 0			# 客户端初始化金钱
CHANGE_MONEY_NORMAL					= 1			# 普通的改变金钱
CHANGE_MONEY_STORE					= 2			# 存入金钱
CHANGE_MONEY_FETCH					= 3			# 取出金钱
CHANGE_MONEY_TEACH_REWARD			= 4			# 徒弟升级，师父获得奖励
CHANGE_MONEY_GEM_HIRE				= 5			# 租用经验宝石所花的金钱
CHANGE_MONEY_BUR_FROM_NPC			= 6			# 从NPC处购买物品
CHANGE_MONEY_FAMILYWAR_BUR_FROM_NPC	= 7			# 家族战场从NPC处购买物品
CHANGE_MONEY_REDEEMITEM				= 8			# 赎回物品
CHANGE_MONEY_BUY_FROM_DARKTRADER	= 9			# 从黑市商人处购买物品
CHANGE_MONEY_TONGABABUYFROMNPC	    = 10		# 购买帮会擂台物品
CHANGE_MONEY_ROLE_TRADING			= 11		# 玩家交易
CHANGE_MONEY_CREATE_FAMILY			= 12		# 创建家族
CHANGE_MONEY_FAMILY_ONCONTESTNPC	= 13		# 家族NPC争夺
CHANGE_MONEY_FAMILY_CHALLENGE		= 14		# 申请“家族挑战”的费用
CHANGE_MONEY_DLGBUYTRAINGEM			= 15		# NPC激活宠物经验石
CHANGE_MONEY_HIRECOMMONGEM			= 16		# 租用宠物经验石
CHANGE_MONEY_REPLYFORMARRIAGE		= 17		# 结婚
CHANGE_MONEY_FORCEDIVORCE			= 18		# 离婚
CHANGE_MONEY_FINDWEDDINGRING		= 19		# 找回结婚戒指
CHANGE_MONEY_SALEGOODS				= 20		# 寄售物品
CHANGE_MONEY_RECEIVESALEITEM		= 21		# 寄卖给玩家发送物品
CHANGE_MONEY_CHAT_YELL				= 22		# 呐喊
CHANGE_MONEY_PROCREATE				= 23		# 宠物繁殖
CHANGE_MONEY_BUYITEMFROMDARKMERCHANT= 24		# 从帮会黑市商人处购买物品
CHANGE_MONEY_CREATETONG				= 25		# 创建帮会
CHANGE_MONEY_REPAIRONEEQUIPBASE		= 26		# 单个装备修理
CHANGE_MONEY_REPAIRALLEQUIPBASE		= 27		# 修理身上所有装备
CHANGE_MONEY_PAYQUESTDEPOSIT		= 28		# 支付随机任务押金
CHANGE_MONEY_MONEYTOYINPIAO			= 29		# 兑换银票
CHANGE_MONEY_MONEYDROPONDIED		= 30		# 死亡掉落
CHANGE_MONEY_STUFFCOMPOSE			= 31		# 材料合成
CHANGE_MONEY_EQUIPSTILETTO			= 32		# 装备打孔
CHANGE_MONEY_EQUIPSPLIT				= 33		# 装备拆分
CHANGE_MONEY_EQUIPINTENSIFY			= 34		# 装备强化
CHANGE_MONEY_EQUIPREBUILD			= 35		# 装备改造
CHANGE_MONEY_EQUIPBIND				= 36		# 装备绑定
CHANGE_MONEY_SPECIALCOMPOSE			= 37		# 特殊合成
CHANGE_MONEY_EQUIPMAKE				= 38		# 装备制造
CHANGE_MONEY_SAVEDOUBLEEXPBUFF		= 39		# 保存双倍奖励BUFF
CHANGE_MONEY_MAIL_SEND				= 40		# 发送邮件
CHANGE_MONEY_VEND_SELL				= 41		# 摆摊购买
CHANGE_MONEY_VEND_SELLPET			= 42		# 宠物摆摊交易
CHANGE_MONEY_LEARN_SKILL			= 43		# 学习技能
CHANGE_MONEY_AUGURY					= 44		# 占卜
CHANGE_MONEY_HUISHIBUKAO			= 45		# 科举会试补考
CHANGE_MONEY_SUANGUAZHANBU			= 46		# 算卦占卜
CHANGE_MONEY_TREASUREMAP			= 47		# 赠送模糊的纸条
CHANGE_MONEY_TELEPORT				= 48		# 传送
CHANGE_MONEY_REACCEPTLOOPQUEST		= 49		# 重接环任务
CHANGE_MONEY_BUKAO					= 50		# 科举补考
CHANGE_MONEY_DEPOSIT				= 51		# 运镖押金
CHANGE_MONEY_SELLTONPC				= 52		# 出售物品给NPC
CHANGE_MONEY_PICKUPMONEY			= 53		# 拾取金币
CHANGE_MONEY_MASTERENDTEACH			= 54		# 师傅解除师徒关系
CHANGE_MONEY_PRENTICEENDTEACH		= 55		# 徒弟解除师徒关系
CHANGE_MONEY_CMS_RECEIVEMONEY		= 56		# 寄卖售出物品
CHANGE_MONEY_SELLITEMTODARKMERCHANT	= 57		# 出售物品给黑市商人
CHANGE_MONEY_RETURNDEPOSIT			= 58		# 返回运镖押金
CHANGE_MONEY_REQUESTADDITEM			= 59		# 任务获取金币
CHANGE_MONEY_MAIL_RECEIVEMONEY		= 60		# 邮件收取金币
CHANGE_MONEY_REWARD_RACEHORSE		= 61		# 赛马奖励
CHANGE_MONEY_DARTREWARD				= 62		# 根据声望领取运镖一周奖励
CHANGE_MONEY_ABANDONED				= 63		# 运镖任务的放弃 返回部分押金
CHANGE_MONEY_QUESTPLACARDGIGHGRADE	= 64		# 精英任务交任务
CHANGE_MONEY_QTTASKKILLDART			= 65		# 杀怪任务获取
CHANGE_MONEY_WIZCOMMAND_SET_MONEY	= 66		# GM命令设置金钱
CHANGE_MONEY_LOTTERYITEM			= 67		# 锦囊抽取
CHANGE_MONEY_WUDAOAWARD				= 68		# 武道大会
CHANGE_MONEY_GROUPQUESTREACCEPT		= 69		# 环任务失败（放弃）重新接受，需要物品或金钱的处理
CHANGE_MONEY_REWARDVALTOMONEY		= 70		# 赏金点换金钱
CHANGE_MONEY_REDPACKAGE				= 71		# 红包获取
CHANGE_MONEY_LUCKYBOXJINBAO			= 72		# 进宝宝盒
CHANGE_MONEY_LUCKYBOXZHAOCAI		= 73		# 招财宝盒
CHANGE_MONEY_QUESTPLACARDGENERAL	= 74		# 普通任务完成任务
CHANGE_MONEY_QTREWARDMONEY			= 75		# 任务奖励
CHANGE_MONEY_QTREWARDROBDARTMONEY	= 76		# 任务奖励
CHANGE_MONEY_QTREWARDMERCHANTMONEY	= 77		# 帮会跑商任务奖励
CHANGE_MONEY_QTREWARDROLELEVELMONEY	= 78		# 任务奖励
CHANGE_MONEY_QTREWARDMONEYFROMTABLE	= 79		# 任务奖励
CHANGE_MONEY_DEPOSIT_RETURN			= 80		# 运镖押金返还
CHANGE_MONEY_PRISON_CONTRIBUTE		= 81		# 监狱捐献
CHANGE_MONEY_OPEN_TREASURE_BOX		= 82		# 开启宝箱
CHANGE_MONEY_DETACHFAMILY			= 83		# 离开帮会后的遣散费(家族退出帮会)
CHANGE_MONEY_MEMBER_LEVEL			= 84		# 离开帮会后的遣散费(成员退出帮会)
CHANGE_MONEY_GETCONTESTNPCMONEY		= 85		# 成员领取NPC管理费
CHANGE_MONEY_CITYTONGMONEY			= 86		# 领取城市占领帮会的管理费
CHANGE_MONEY_RECEIVETSPET			= 87		# 获得寄售的宠物
CHANGE_MONEY_CITYWAR_OVER			= 88		# 城战结束给与奖励
CHANGE_MONEY_RECEIVETSITEM			= 89		# 获得寄售的物品
CHANGE_MONEY_BUYCOLLECTIONITEM		= 90		# 支付收购物品押金（买方）
CHANGE_MONEY_SELLCOLLECTIONITEM		= 91		# 出售收购物品获得（卖方）
CHANGE_MONEY_CANCELCOLLECTIONITEM	= 92		# 取回收购物品押金（买方）
CHANGE_MONEY_CMS_SOCKS				= 93		# 圣诞袜子
CHANGE_MONEY_TONG_CITYWAR_SIGNUP	= 94		# 城战竞拍费用
CHANGE_MONEY_YYD_BOX				= 95		# 打开荣誉宝盒
CHANGE_MONEY_UPDATE_COLLECTION_ITEM_INFO	= 96	#更改收购物品信息引起的金钱变化
CHANGE_MONEY_TALISMAN_SPLIT			= 97		# 法宝分解引起金钱变化 by 姜毅
CHANGE_MONEY_BUY_YUANBAO			= 98		# 购买元宝 by 姜毅
CHANGE_MONEY_CANCLE_YBT_BILL		= 99		# 撤销订单 by 姜毅
CHANGE_MONEY_DRAW_MONEY				= 100		# 交易账号取出金钱 by 姜毅
CHANGE_MONEY_RECEIVETS_MONEY		= 101		# 获得寄售的金钱
CHANGE_MONEY_EQUIP_EXTRACT			= 102		# 装备属性抽取引起的金钱变化
CHANGE_MONEY_EQUIP_POUR				= 103		# 装备属性灌注引起的金钱变化
CHANGE_MONEY_EQUIP_UP				= 104		# 装备飞升
CHANGE_MONEY_RABBIT_RUN				= 105		# 小兔快跑
CHANGE_MONEY_ALLY					= 106		# 结拜
CHANGE_MONEY_TONG_CONTRIBUTE		= 107		# 帮会捐献
CHANGE_MONEY_POINT_CARD_YAJIN		= 108		# 寄售点卡押金
CHANGE_MONEY_FCWR_FOR_MSG			= 109		# 非诚勿扰发言费
CHANGE_MONEY_LIVING_LEVEL_UP_SKILL	= 110		# 升级生活技能收费
CHANGE_MONEY_NPC_TALK				= 111		# 与NPC对话收费
# ---------------------------------------------------------------------
# 改变潜能的方式	by姜毅
# ---------------------------------------------------------------------
CHANGE_POTENTIAL_INITIAL			= 0			# 默认方式
CHANGE_POTENTIAL_FABAO				= 1			# 法宝吸取潜能 by姜毅
CHANGE_POTENTIAL_ZHAOCAI			= 2			# 招财宝盒
CHANGE_POTENTIAL_JINBAO				= 3			# 进宝宝盒
CHANGE_POTENTIAL_CITYWAR_OVER		= 4			# 城战结束给与奖励
CHANGE_POTENTIAL_ROBOT_VERIFY		= 5			# 反外挂验证答题奖励
CHANGE_POTENTIAL_CMS_SOCKS			= 6			# 圣诞袜子
CHANGE_POTENTIAL_OLD_PLAYER_REWARD	= 7			# 老玩在线定时奖励
CHANGE_POTENTIAL_YYD_BOX			= 8			# 打开荣誉宝盒
CHANGE_POTENTIAL_USE_ITEM			= 9			# 使用潜能丹
CHANGE_POTENTIAL_USE_ITEM_2			= 10		# 使用潜能物品

# ---------------------------------------------------------------------
# 改变元宝的方式
# ---------------------------------------------------------------------
CHANGE_GOLD_INITIAL					= 0
CHANGE_GOLD_NORMAL					= 1
CHANGE_SILVER_INITIAL				= 0
CHANGE_SILVER_NORMAL				= 1

# ---------------------------------------------------------------------
# 角色行为
# ---------------------------------------------------------------------
ROLE_TALK_BEHAVIOR 					= 0						# 对话中
ROLE_SPELL_BEHAVIOR 				= 1						# 吟唱中
ROLE_PICK_DROPBOX_BEHAVIOR			= 2						# 拾取普通箱子行为
ROLE_PICK_QUESTBOX_BEHAVIOR			= 3						# 拾取任务箱子行为


# ---------------------------------------------------------------------
# 改变功勋值的方式
# ---------------------------------------------------------------------
CHANGE_TEACH_CREDIT_NORMAL		= 0			# 普通的改变功勋值
CHANGE_TEACH_CREDIT_REWARD		= 1			# 徒弟升级，师父获得奖励



#----------------------------------------------------------------------
#游戏奖励类型
#----------------------------------------------------------------------
REWARD_NONE						= 0							#无类型
REWARD_TEAMCOMPETITION_EXP		= 1 						#组队竞赛经验奖励
REWARD_TEAMCOMPETITION_ITEMS	= 2 						#组队竞赛物品奖励
REWARD_RACE_HORSE				= 3							#赛马奖励
REWARD_RACE_HORSE_ITEMS			= 4							#赛马物品奖励
REWARD_RABBIT_RUN				= 5							#小兔快跑
REWARD_TONG_ABA					= 6							#帮会擂台赛冠军物品奖励
REWARD_TEAMCOMPETITION_POT		= 7							#组队竞技潜能奖励
REWARD_TEAMCOMPETITION_BOX_EXP	= 8							#组队竞技箱子经验奖励
REWARD_ROLECOMPETITION_EXP		= 9							#个人竞技经验奖励
REWARD_ROLECOMPETITION_BOX_EXP	= 10						#个人竞技箱子经验奖励
REWARD_TONG_ABA_EXP				= 11						#帮会擂台赛获胜经验奖励
REWARD_TONG_WUDAO				= 12						#武道在会奖励
REWARD_TONG_TEAM_CHALLENGE		= 13						#组队擂台奖励
REWARD_TONG_TONG_CITY_WAR		= 14						#帮会城市战奖励

#----------------------------------------------------------------------
# 箱子掉落方式
#----------------------------------------------------------------------
DROPPEDBOX_TYPE_NONE			= 0							# 默认掉落方式
DROPPEDBOX_TYPE_MONSTER			= 1 						# 怪物掉落
DROPPEDBOX_TYPE_STROE			= 2 						# 开宝箱掉落
DROPPEDBOX_TYPE_OTHER			= 3							# 其他掉落

# ---------------------------------------------------------------------
# addProxyData - onProxyDataDownloadComplete（详见api文档）
# 通信机制消息id定义，20:43 2009-7-7，wsf
# ---------------------------------------------------------------------
PROXYDATA_CSOL_VERSION		= 1						# 创世当前版本数据

#----------------------------------------------------------------------
#帮会金钱改变的原因
#----------------------------------------------------------------------
TONG_CHANGE_MONEY_NORMAL				=	0	# 默认
TONG_CHANGE_MONEY_DETACHFAMILY			=	1	# 家族脱离，遣散费
TONG_CHANGE_MONEY_PAYFIRSTCHIEF			=	2	# 帮主工资发放
TONG_CHANGE_MONEY_PAYSECONDCHIEF		=	3	# 副帮主工资发放
TONG_CHANGE_MONEY_SUBMITCONTESTMONEY	=	4	# 申请城市争夺战
TONG_CHANGE_MONEY_CITYWARBUYMACHINE		=	5	# 夺城战购买机械
TONG_CHANGE_MONEY_BUILDCITYWARTOWER		=	6	# 建造城战塔楼
TONG_CHANGE_MONEY_ROBWARFAILED			=	7	# 帮会在掠夺战中失败
TONG_CHANGE_MONEY_RESEARCHSKILL			=	8	# 研发一个帮会技能
TONG_CHANGE_MONEY_CHANGEMAKEITEMS		=	9	# 改变帮会正在研发的物品
TONG_CHANGE_MONEY_PAYSPENDMONEY			=	10	# 领地维护费
TONG_CHANGE_MONEY_BUILDTONGBUILDING		=	11	# 申请修建帮会建筑
TONG_CHANGE_MONEY_SELECTSHOUSHOU		=	12	# 选择帮会神兽
TONG_CHANGE_MONEY_ROBWARSUCCESSFULLY	=	13	# 帮会在掠夺战中胜利
TONG_CHANGE_MONEY_BACKOUTTONGBUILDING	=	14	# 帮会建筑中反悔,拆除已修建的建筑返还部分资金
TONG_CHANGE_MONEY_GMCOMMAND				=	15	# GM命令增加帮会金钱
TONG_CHANGE_MONEY_QTREWARDMERCHANTMONEY	=	16	# 帮会跑商
TONG_CHANGE_MONEY_QTREWARDTONGMONEY		=	17	# 帮会贡献任务
TONG_CHANGE_MONEY_CITY_REVENUE			=	18	# 帮会领取城市税收
TONG_CHANGE_MONEY_SUBMIT_TONG_SIGN		=	19	# 帮会上传会标
TONG_CHANGE_MONEY_CHANGE_TONG_SIGN		=	20	# 帮会更换会标
TONG_CHANGE_MONEY_CONTRIBUTE_TO			=	21	# 帮会捐献金钱
TONG_CHANGE_MONEY_ITEM						= 22	# 帮会资金改变道具
TONG_CHANGE_MONEY_ABATTOIR				= 	23	# 帮会擂台赛报名费
TONG_CHANGE_MONEY_SALARY				=	24	# 帮众领取帮会俸禄
TONG_CHANGE_MONEY_CITY_WAR_INTEGAL		= 	25	# 帮会城市战决赛积分兑换
#---------------------------------------------------------------------
#删除物品的原因
#---------------------------------------------------------------------
DELETE_ITEM_NORMAL						=	0	# 默认
DELETE_ITEM_STACK						=	1	# 叠加物品（物品合并）
DELETE_ITEM_STOREITEM					=	2	# 存储物品
DELETE_ITEM_EQUIPSTILETTO				=	3	# 装备打孔
DELETE_ITEM_CLEARWARITEMS				=	4	# 清除家族战争玩家身上的战场物品
DELETE_ITEM_WARITEMSDROPONDIED			=	5	# 家族战争死亡掉落战场物品
DELETE_ITEM_CLEARABAITEMS				=	6	# 清除家族挑战赛玩家身上的战场物品
DELETE_ITEM_ABAITEMSDROPONDIED			=	7	# 家族挑战赛死亡掉落战场物品
DELETE_ITEM_COMMAND_CLEANBAGS			=	8	# GM命令清空包裹
DELETE_ITEM_COMMAND_SWEAR				=	9	# 结为恋人删除同心结
DELETE_ITEM_WELKINYELL					=	10	# 天音符广播删除天音符
DELETE_ITEM_TUNNELYELL					=	11	# 地音符广播删除地音符
DELETE_ITEM_REMOVE_BC_CARDS				=	12	# (作废)删除变身大赛卡片
DELETE_ITEM_PET_ADDLIFE					=	13	# 宠物增加寿命值删除延寿丹
DELETE_ITEM_PET_ADDJOYANCY				=	14	# 宠物增加快乐度删除一个溜溜球或布娃娃
DELETE_ITEM_PET_ENHANCE					=	15	# 宠物强化删除(玩家使用宠物强化功能时，删除该功能所需要的物品)
DELETE_ITEM_SUPERKLJDACTIVITY			=	16	# （未完成的功能）超级砸蛋奖励删除超级大奖的物品
DELETE_ITEM_KLJDEXPREWARD				=	17	# （未完成的功能）快乐金蛋，获取经验奖励,删除快乐金蛋经验奖励物品
DELETE_ITEM_KLJDZUOQIREWARD				=	18	# （未完成的功能）快乐金蛋，获取坐骑奖励,删除快乐金蛋坐骑奖励物品
DELETE_ITEM_CLEARCITYWARFUNGUS			=	19	# 在玩家离开战场时要清除身上的蘑菇物品
DELETE_ITEM_TONG_STOREITEM				=	20	# 存储到帮会仓库
DELETE_ITEM_TONG_ANGELEXCHANGE			=	21	# (作废)仙灵换物
DELETE_ITEM_DROPONDIED					=	22	# 玩家死亡掉落
DELETE_ITEM_STUFFCOMPOSE				=	23	# 材料合成
DELETE_ITEM_EQUIPSPLIT					=	24	# 装备拆分
DELETE_ITEM_EQUIPSTUDDED				=	25	# 装备镶嵌
DELETE_ITEM_EQUIPINTENSIFY				=	26	# 装备强化
DELETE_ITEM_EQUIPREBUILD				=	27	# 装备改造
DELETE_ITEM_EQUIPBIND					=	28	# 装备绑定
DELETE_ITEM_SPECIALCOMPOSE				=	29	# 特殊合成
DELETE_ITEM_EQUIPMAKE					=	30	# 装备制造
DELETE_ITEM_USEFLY						=	31	# 使用引路蜂
DELETE_ITEM_USEITEMREVIVE				=	32	# 用复活物品复活
DELETE_ITEM_USE							=	33	# 使用物品（自身可使用的物品，因为玩家的使用行为而删除；例如：使用药水瓶）
DELETE_ITEM_DIGTREASURE					=	34	# 挖掘藏宝图（因为挖宝藏的行为被删除的物品；例如：锄头）
DELETE_ITEM_FISHINGINCHARGE				=	35	# 渔场垂钓卡换取钓鱼时间
DELETE_ITEM_CIFU						=	36	# 祈求海神赐福
DELETE_ITEM_XIANLING					=	37	# 祈求龙王显灵
DELETE_ITEM_JIANZHENG					=	38	# 龙王见证情缘
DELETE_ITEM_PEARLPRIME					=	39	# 吸取珍珠精华
DELETE_ITEM_CREATETONGRACEHORSE			=	40	# 创建帮会赛马
DELETE_ITEM_SHITUREWARD					=	41	# 师徒关怀换取奖励
DELETE_ITEM_SHITUCHOUJIANG				=	42	# 师徒关怀抽奖
DELETE_ITEM_RECORDRANQUEST				=	43	# 记录随机任务
DELETE_ITEM_ABANDONEDQUESTRANDOM		=	44	# 放弃任务
DELETE_ITEM_ABANDONEDQUESTLOOP			=	45	# 环任务放弃任务
DELETE_ITEM_COMPLETEQTTASKSUBMIT		=	46	# 普通提交任务删除提交物品
DELETE_ITEM_COMPLETESUBMITPICTURE		=	47	# 提交画像,删除物品(画像任务)
DELETE_ITEM_COMPLETESUBMIT_YINPIAO		=	48	# 提交银票,删除物品(跑商任务)
DELETE_ITEM_BCREWARD					=	49	# 领取变色大赛奖励,移除掉玩家身上的纸牌
DELETE_ITEM_SELLTONPC					=	50	# 出售给NPC
DELETE_ITEM_ROLETRADING					=	51	# 玩家交易
DELETE_ITEM_DESTROYITEM					=	52	# 销毁物品
DELETE_ITEM_SALEGOODS					=	53	# 寄售一个物品
DELETE_ITEM_PAYYINPIAO					=	54	# 购买物品 支付银票
DELETE_ITEM_ACTIVATEBAG					=	55	# 激活包裹
DELETE_ITEM_USEVEHICLEITEM				=	56	# 使用骑宠物品
DELETE_ITEM_USEVEHICLEEQUIP				=	57	# 使用骑宠装备
DELETE_ITEM_ITEMLIFEOVER				=	58	# 有生命物品生命周期到达（有过期时间的物品过期时自动删除）
DELETE_ITEM_MAIL_SEND					=	59	# 玩家邮寄物品
DELETE_ITEM_VEND_SELL					=	60	# 摆摊出售
DELETE_ITEM_SPELLTOITEM					=	61	# 同“DELETE_ITEM_USE”，但使用目标是另一个物品。
DELETE_ITEM_BUYFROMDARKTRADER			=	62	# 购买物品,消耗玩家商会卷轴
DELETE_ITEM_BUYFROMITEMCHAPMAN			=	63	# 玩家用物品换物品（在NPC商人处，用一定数量的某类物品购买商人的其它物品）
DELETE_ITEM_SYS_RECLAIM_ITEM			=	64	# 系统回收玩家物品(玩家的某些行为导致系统删除某些特定的物品；例如玩家离开某个区域或触发了某个场景物件)
DELETE_ITEM_PAY							=	65	# 使用技能消耗物品
DELETE_ITEM_COMMITCITYWAR				=	66	# 提交城市战场任务
DELETE_ITEM_QTSREMOVEITEM				=	67	# 任务删除物品
DELETE_ITEM_QTSCHECKITEM				=	68	# 任务删除物品
DELETE_ITEM_QTSAFTERDELETEITEM			=	69	# 放弃任务后，删除相应的任务物品
DELETE_ITEM_QTTASKDELIVER				=	70	# 收集任务，完成任务后删除
DELETE_ITEM_STACKABLEITEM				=	79	# 物品进背包一部分数量被被拆分叠加到另一堆物品上
DELETE_ITEM_AUTOINSTUFFS				=	80	# 装备打造中，自动合并并拆分需求的材料
DELETE_ITEM_STACKABLEINBAG				=	81	# 背包内叠加物品
DELETE_ITEM_DOMESTICATEVEHICLE			=	82	# 置骑宠快乐度状态
DELETE_ITEM_FEEDVEHICLE					=	83	# 骑宠喂食
DELETE_ITEM_UPDATETALISMANGRADE			=	84	# 提升法宝品质
DELETE_ITEM_ACTIVATETALISMANATTR		=	85	# 激活法宝属性
DELETE_ITEM_REBUILDTALISMANATTR			=	87	# 改造法宝属性
DELETE_ITEM_ADDTALISMANLIFE				=	88	# 法宝充值（对于有使用期限的法宝，增加法宝的使用期限时所用到的冲值道具的删除）
DELETE_ITEM_USEYAODING					=	89	# 使用灵药鼎
DELETE_ITEM_ITEMTELEPORT				=	90	# 使用传送卷轴， 记忆蝴蝶 道具
DELETE_ITEM_SPLITITEM					=	91	# 拆分物品
DELETE_ITEM_COMBINEITEM					=	92	# 物品合并
DELETE_ITEM_COLLECTION					=	93	# 收购物品
DELETE_ITEM_TALISMAN_SPLIT				=	94	# 法宝分解
DELETE_ITEM_TALISMAN_INTENSIFY			=	95	# 法宝强化
DELETE_ITEM_COLLECT_POINT				=	96	# 采集
DELETE_ITEM_GOD_WEAPON_MAKE				=	97	# 神器
DELETE_ITEM_ALLY						=	98	# 结拜
DELETE_ITEM_LUCKY_BOX					=	99	# 天降宝盒
DELETE_ITEM_POTENTIAL_BOOK				= 	100	# 使用潜能书
DELETE_ITEM_MONSTER_ATTACK				=	101	# 怪物攻城
DELETE_ITEM_MEGRA_FRUIT					=	102	# 七夕活动魅力果实合成移除
DELETE_ITEM_EQUIP_EXTRACT				=	103	# 装备抽取
DELETE_ITEM_EQUIP_POUR					=	104	# 装备灌注
DELETE_ITEM_EQUIP_UP					=	105 # 装备飞升
DELETE_ITEM_EQUIP_ATTR_REBUILD			=	106 # 装备属性重铸
DELETE_ITEM_RABBIT_RUN					= 	107	# 小兔快跑
DELETE_ITEM_REMOVECRYSTAL				=	108	# 水晶摘除
DELETE_ITEM_CHANGEPROPERTY				=	109 # 洗前缀
DELETE_ITEM_IMPROVEQUALITY				=	110 # 绿装升品
DELETE_ITEM_USENATRUEJADEITEM			=	111 # 使用造化玉牒
#-------------------------------------------------------------------------------
#获得物品的原因
#-------------------------------------------------------------------------------
ITEM_NORMAL								=	0	# 默认
ADD_ITEM_BUYFROMNPC						=	1	# 从NPC处购买
ADD_ITEM_BUYFROMITEMCHAPMAN				=	2	# （废弃）从NPC那，玩家用物品换物品
ADD_ITEM_ROLE_TRADING					=	3	# 玩家交易
ADD_ITEM_RECEIVESPECIALGOODS			=	4	# 商城购买物品
ADD_ITEM_ADDLOTTERYITEM					=	5	# 增加锦囊奖励物品
ADD_ITEM_EQUIPSTUDDED					=	6	# 装备镶嵌（代码已改为85，谁干的？）
ADD_ITEM_EQUIPINTENSIFY					=	7	# 装备强化（代码已改为86，谁干的？）
ADD_ITEM_REQUESTADDITEM					=	8	# 拾取物品
ADD_ITEM_ONLINEBENEFI					=	9	# 在线累计时间奖励
ADD_ITEM_QUESTDART						=	10	# 运镖任务奖励
ADD_ITEM_QTREWARDITEMS					=	11	# 普通任务物品奖励（一个或多个）
ADD_ITEM_QTREWARDCHOOSEITEMS			=	12	# 多选一物品奖励
ADD_ITEM_QTREWARDRNDITEMS				=	13	# 随机物品奖励（随机得到一个或多个）
ADD_ITEM_QTREWARDFIXEDRNDITEMS			=	14	# 随机物品奖励（必然得到多个中的某一个）
ADD_ITEM_QTREWARDRANDOMITEMFROMTABLE	=	15	# 随机奖励（奖励物品来源于一个表格）
ADD_ITEM_QUESTTONGNORMALLOOPGROUP		=	16	# 帮会环任务奖励
ADD_ITEM_QUESTTONGFETEGROUP				=	17	# 帮会祭祀物品奖励
ADD_ITEM_QUESTTONGBUILDGROUP			=	18	# 帮会建设物品奖励
ADD_ITEM_QTSGIVEITEMS					=	19	# 任务给予物品，完成任务需要用到
ADD_ITEM_QTSGIVEYINPIAO					=	20	# 任务给予银票，如跑商任务
ADD_ITEM_QUEST							=	21	# 通过任务得到一些普通物品
ADD_ITEM_QUESTPOTENTIAL					=	22	# 潜能任务奖励
ADD_ITEM_GMCOMMAND						=	23	# GM命令增加
ADD_ITEM_MASTERENDTEACH					=	24	# 解除师徒关系
ADD_ITEM_REPLYFORMARRIAGE				=	25	# 玩家结婚,领取结婚戒指(女)
ADD_ITEM_MARRYSUCCESS					=	26	# 玩家结婚,领取结婚戒指(男)
ADD_ITEM_FINDWEDDINGRING				=	27	# 玩家申请找回结婚戒指
ADD_ITEM_RECEIVESALEITEM				=	28	# 购买寄卖物品
ADD_ITEM_RECEIVECANCELITEM				=	29	# 玩家取消寄卖物品
ADD_ITEM_TONG_ANGELEXCHANGE				=	30	# （作废）仙灵换物
ADD_ITEM_ADDWUDAOAWARD					=	31	# 武道大会奖励
ADD_ITEM_TAKEPRESENT					=	32	# 运营活动奖励，当前包括来自于“custom_ChargePresentUnite”和“custom_ItemAwards”表的奖励。
ADD_ITEM_EQUIPMAKE						=	33	# 装备制造
ADD_ITEM_VEND_SELL						=	34	# 摆摊购买物品
ADD_ITEM_LUCKYBOXJINBAO					=	35	# 进宝宝盒掉落
ADD_ITEM_CHINAJOY_VIP					=	36	# 从CHINAJOY的VIP礼包获得，根据性别、职业获得一批奖励
ADD_ITEM_TREASUREMAP					=	37	# 藏宝图掉落
ADD_ITEM_LUCKYBOXZHAOCAI				=	38	# 招财宝盒掉落
ADD_ITEM_CHINAJOY						=	39	# 从CHINAJOY礼包获得，只奖励一个物品
ADD_ITEM_FISHING						=	40	# 钓鱼获得
ADD_ITEM_PRESENTKIT						=	41	# 从礼品包获得，一个比较通用的通过使用一个物品获得一批物品的奖励方式
ADD_ITEM_TEAMCOMPETITION				=	42	# 组队竞赛物品奖励
ADD_ITEM_DARTREWARD						=	43	# 声望领取运镖一周奖励
ADD_ITEM_TESTACTIVITYGIFT				=	44	# 封测等级奖励（凤鸣城中心NPC处领取的10-50级等级奖励）
ADD_ITEM_SPREADERGIFT					=	45	# 推广员奖励（简体版专用）
ADD_ITEM_SHITUCHOUJIANG					=	46	# 师徒关怀抽奖
ADD_ITEM_CHUSHIREWARD					=	47	# 师徒关怀30和45级出师奖励
ADD_ITEM_FAMILYWARBUYFROMNPC			=	48	# 购买家族战场物品
ADD_ITEM_REDEEMITEM						=	49	# 赎回物品
ADD_ITEM_BUYFROMDARKTRADER				=	50	# 玩家买物品
ADD_ITEM_BUYFROMTONGNPC					=	51	# （作废）帮会NPC处购买物品
ADD_ITEM_TONGABABUYFROMNPC				=	52	# 购买帮会擂台物品
ADD_ITEM_PICKUPITEM						=	53	# 拾取一个物品Entity
ADD_ITEM_DOKLJDREWARD					=	54	# （未完成的功能）砸蛋奖励
ADD_ITEM_DOSUPERKLJDACTIVITY			=	55	# （未完成的功能）超级砸蛋奖励
ADD_ITEM_DOKLJDZUOQIREWARD				=	56	# （未完成的功能）快乐金蛋，获取坐骑奖励
ADD_ITEM_BUYITEMFROMMERCHANT			=	57	#  玩家从特产商人处购买的物品
ADD_ITEM_BUYITEMFROMDARKMERCHANT		=	58	#  玩家从黑市商人处购买的物品
ADD_ITEM_CHANGEGOLDTOITEM				=	59	# 银票替换为金元宝票物品
ADD_ITEM_GETCITYTONGITEM				=	60	# 领取城市占领帮会的经验果实
ADD_ITEM_ADDQUESTITEM					=	61	# 从场景物件上获得的物品（翻箱子、尸体等）
ADD_ITEM_RECEIVEITEM					=	62	# 获取邮件附带物品
ADD_ITEM_ONREWARDITEM					=	63	# （作废）赏金点数奖励装备
ADD_ITEM_ITEMTELEPORT					=	64	# 使用传送物品获得（记忆蝴蝶一类的物品使用后生成）
ADD_ITEM_ITEM_PICTURE					=	65	# 使用画画卷轴
ADD_ITEM_CIFU							=	66	# 海神赐福获取：日光浴场功能之一，以与NPC对话的方式，物品换物品功能
ADD_ITEM_XIANLING						=	67	# 龙王显灵获取：日光浴场功能之一，以与NPC对话的方式，物品换物品功能
ADD_ITEM_JIANZHENG						=	68	# 龙王见证情缘获取：日光浴场功能之一，以与NPC对话的方式，物品换物品功能
ADD_ITEM_POTENTIALMELEE					=	69	# 潜能乱斗奖励
ADD_ITEM_TAKELINGYAO					=	70	# 领取灵药
ADD_ITEM_LOGINBCGAME					=	71	# 参加变身大赛获取
ADD_ITEM_BCREWARD						=	72	# 变身大赛领取奖励
ADD_ITEM_STOREITEM						=	73	# 从银行取出(存储物品时，放到了有物品的格子上，导致物品交换)
ADD_ITEM_ADDITEM2ORDER					=	74	# 从银行取出（把一个物品放到指定包裹的指定格子中）
ADD_ITEM_FETCHITEM2KITBAGS				=	75	# 从银行取出（把一个物品放到指定包裹中）
ADD_ITEM_TONG_STOREITEM					=	76	# 帮会银行中取出(存储物品时，放到了有物品的格子上，导致物品交换)
ADD_ITEM_TONG_FETCHITEM					=	77	# 帮会银行中取出(把一个物品放到指定包裹中)
ADD_ITEM_TONG_ADDITEM2ORDER				=	78	# 帮会银行中取出（把一个物品放到指定包裹的指定格子中）
ADD_ITEM_STACKABLEITEM					=	79	# 物品进背包,叠加到现有物品上
ADD_ITEM_STACKABLEINBAG					=	81	# 背包内叠加物品
ADD_ITEM_COMBINEITEM					=	82	# 物品合并（数量增加）
ADD_ITEM_EQUIPSTILETTO					=	83	# 装备打孔
ADD_ITEM_STUFFCOMPOSE					=	84	# 材料合成
ADD_ITEM_EQUIPSTUDDED					=	85	# 装备镶嵌
ADD_ITEM_EQUIPINTENSIFY					=	86	# 装备强化
ADD_ITEM_EQUIPREBUILD					=	87	# 装备改造
ADD_ITEM_EQUIPBIND						=	88	# 装备绑定
ADD_ITEM_SPECIALCOMPOSE					=	89	# 特殊合成
ADD_ITEM_REQUESTIEEXP					=	90	# 科举考试奖励
ADD_ITEM_REQUESTIETITLE					=	91	# 领取称号奖励
ADD_ITEM_RACEHORSE						=	92	# 赛马物品奖励
ADD_ITEM_TAKEJKCARDPRESENT				=	93	# 领取新手卡奖励（简体版专用）
ADD_ITEM_FIXITMEREWARD					=	94	# 新角色计时奖励
ADD_ITEM_TREASURE_BOX					=	95	# 开启宝箱
ADD_ITEM_STACK							=	96	# 拖动一个可叠加物叠加到物品上
ADD_ITEM_GIVEFIXTIMEREWARD				=	97	# 通过直接给与或者邮件方式给与玩家实时新手奖励
ADD_ITEM_SPLITITEM						=	98	# 拆分物品获取拆分出的物品
ADD_ITEM_AUTOINSTUFFS					=	99	# 装备打造自动拆分后获取拆分出的物品
ADD_ITEM_CMS_SOCKS						= 	100	# 圣诞袜子（与招财、进宝宝箱相同功能的物品）
ADD_ITEM_OLD_PLAYER_REWARD				=	101	# 老手在线奖励 by 姜毅
ADD_ITEM_FOR_HONOR_GIFT					= 	102	# 荣誉度宝盒（以与NPC对话的方式，使用荣誉值换取的宝盒）
ADD_ITEM_YYD_BOX						= 	103	# 打开荣誉宝盒
ADD_ITEM_EQUIPSPLIT						=	104	# 装备拆分
ADD_ITEM_ROB_WAR_ITEM					=	105	# 掠夺战领取奖励
ADD_ITEM_SALEGOODS						=	106 # 从替售NPC出获得物品，包括买入和取出
ADD_ITEM_COLLECTION						=	107	# 收购物品（从我的替售NPC上获取成功收购的物品）
ADD_ITEM_TALISMAN_SPLIT					=	108	# 法宝分解
ADD_ITEM_TALISMAN_INTENSIFY				=	109	# 法宝强化
ADD_ITEM_COLLECT_POINT					=	110	# 采集所得
ADD_ITEM_TEACH_SUCCESS					=	111	# 拜师成功
ADD_ITEM_GOD_WEAPON_MAKE				=	112	# 神器炼制
ADD_ITEM_SPRING_FESTIVAL				=	113	# 春节活动
ADD_ITEM_USE							=	114	# 使用物品得到另一个物品
ADD_ITEM_DANCE							=	115	# 跳舞
ADD_ITEM_CHANGE_BODY					=	116	# 变身
ADD_ITEM_TANABATA_QUIZ					=	117	# 七夕问答物品奖励
ADD_ITEM_SKILL_CLEAR					=	118	# 洗技能物品获得
ADD_ITEM_FRUIT_TREE						=	119	# 七夕活动魅力果树采集获得
ADD_ITEM_MEGRA_FRUIT					=	120	# 七夕活动魅力果实合成获得
ADD_ITEM_GET_FRUIT						=	121	# 七夕活动魅力果实领取种子
ADD_ITEM_EQUIP_EXTRACT					= 	122	# 装备抽取
ADD_ITEM_RABBIT_RUN						= 	123	# 小兔快跑活动获得物品
ADD_ITEM_REMOVECRYSTAL					=	124	# 水晶摘除
ADD_ITEM_CHANGEPROPERTY					=	125	#洗前缀
ADD_ITEM_IMPROVEQUALITY					=	126	#绿装升品

# ---------------------------------------------------------------------
# entity缓冲器任务类型定义( kebiao, 2009-7-15 )
# ---------------------------------------------------------------------
ENTITY_CACHE_TASK_TYPE_NPCOBJECT0						= 0x00000000			# 获取npcobject模块相关的属性
ENTITY_CACHE_TASK_TYPE_NPCOBJECT1						= 0x00000011			# 获取城主NPC模块相关的属性
ENTITY_CACHE_TASK_TYPE_MONSTER0							= 0x00000010			# 获取monster模块相关的属性
ENTITY_CACHE_TASK_TYPE_COMBATUNIT0						= 0x00000020			# 获取combatunit模块相关的属性
ENTITY_CACHE_TASK_TYPE_PET0								= 0x00000030			# 获取pet模块相关的属性=======





# ---------------------------------------------------------------------
# 活动已记录标志（也就是不可参加标记）
# ---------------------------------------------------------------------
ACTIVITY_FLAGS_TIANGUAN					= 0		#天关
ACTIVITY_FLAGS_SHUIJING					= 1		#水晶
ACTIVITY_FLAGS_RACEHORSE				= 2		#赛马
ACTIVITY_FLAGS_YAYU						= 3		#猰貐
ACTIVITY_FLAGS_XLDX						= 4		#邪龙洞穴
ACTIVITY_FLAGS_CMS_SOCKS				= 5		#圣诞树袜子
ACTIVITY_FLAGS_MEMBER_DART				= 6		#成员运镖
ACTIVITY_FLAGS_SPRING_RIDDLE			= 7		# 春节灯谜
ACTIVITY_FLAGS_FJSG						= 8		#封剑神宫
ACTIVITY_FLAGS_SHMZ						= 9		#摄魂迷阵
ACTIVITY_FLAGS_TEACH_REWARD				= 10	#师徒每日奖励
ACTIVITY_FLAGS_FCWR						= 11	#非诚勿扰发公告记录
ACTIVITY_FLAGS_TANABATA_QUIZ			= 12	# 玩家七夕问答
ACTIVITY_FLAGS_RABBITRUN				= 13	# 小兔快跑
ACTIVITY_FLAGS_KUAFUREMAIN				= 14	# 夸父神殿
ACTIVITY_FLAGS_TONG_FUBEN				= 15	#帮会副本任务
ACTIVITY_FLAGS_CHALLENGE_FUBEN			= 16	#挑战副本
ACTIVITY_FLAGS_CHALLENGE_FUBEN_MANY		= 17 	#挑战副本，三人
ACTIVITY_FLAGS_SHENGUIMIJING			= 18	# 神鬼秘境
ACTIVITY_FLAGS_WUYAOQIANSHAO			= 19	# 巫妖前哨
ACTIVITY_FLAGS_WUYAOWANGBAOZANG			= 20	# 失落宝藏(巫妖王宝藏)
ACTIVITY_FLAGS_JYLD						= 21	# 经验乱斗
ACTIVITY_FLAGS_QNLD						= 22	# 潜能乱斗
ACTIVITY_FLAGS_YING_XIONG_LIAN_MENG		= 23	#英雄联盟


# ---------------------------------------------------------------------
# 需要存储的标志为
# ---------------------------------------------------------------------
ENTITY_FLAG_KUA_FU_QUEST				= 0		# 拥有夸父神殿任务

#-----------------------------------------------------------------------
#增加经验的原因
#-----------------------------------------------------------------------
CHANGE_EXP_INITIAL							= 0		# 客户端初始化经验
CHANGE_EXP_ABA								= 1		# 获得擂台赛经验奖励
CHANGE_EXP_COMMAND							= 2		# GM命令增加经验
CHANGE_EXP_MASTERENDTEACH					= 3		# 解除师徒关系
CHANGE_EXP_PRENTICEENDTEACH					= 4		# 玩家出师
CHANGE_EXP_TEACH_REWARD						= 5		# 徒弟升级，师父获得奖励
CHANGE_EXP_FABAO							= 6		# 法宝吸取经验 by姜毅
CHANGE_EXP_QUIZ_GAMEOVER					= 7		# 玩家答题接受奖励
CHANGE_EXP_TRAINEXPGEM						= 8 	# 玩家吸收代练宝石经验
CHANGE_EXP_BCNPC							= 9		# NPC变身大赛获取经验
CHANGE_EXP_KLJDEXPREWARD					= 10	# 快乐金蛋经验奖励
CHANGE_EXP_KEJUREWARD						= 11	# 科举考试奖励经验
CHANGE_EXP_WUDAOAWARD						= 12	# 武道大会奖励经验
CHANGE_EXP_KILLMONSTER						= 13	# 杀怪获取经验
CHANGE_EXP_REWARDVALTOEXP					= 14	# 赏金点换取经验
CHANGE_EXP_DANCE							= 15	# 跳舞获得经验
CHANGE_EXP_LUCKYBOXJINBAO					= 16	# 进宝宝盒获取经验
CHANGE_EXP_DOUBLEDANCE						= 17	# 双人舞获得经验
CHANGE_EXP_TROOPDANCE						= 18	# 组队跳舞获得经验
CHANGE_EXP_SUN_BATH							= 19	# 日光浴获取经验
CHANGE_EXP_LUCKYBOXZHAOCAI					= 20	# 招财宝盒获取经验
CHANGE_EXP_TEAMCOMPETITION					= 21	# 组队竞赛经验奖励
CHANGE_EXP_RACEHORSE						= 22	# 赛马奖励
CHANGE_EXP_DARTREWARD						= 23	# 根据声望领取运镖一周奖励
CHANGE_EXP_PEARLPRIME						= 24	# 吸取珍珠精华
CHANGE_EXP_CHUSHIREWARD						= 25	# 师徒关怀30和45级出师奖励(必须由徒弟来领取)
CHANGE_EXP_PLACARDGIGHGRADE					= 26	# 精英任务交任务
CHANGE_EXP_PLACARDGENERAL					= 27	# 普通任务交任务
CHANGE_EXP_QTREWARD							= 28	# 交任务获得经验
CHANGE_EXP_QTREWARDPERCENTAGEEXP			= 29	# 交任务获得经验
CHANGE_EXP_QTREWARDRELATIONEXP				= 30	# 交任务获得经验
CHANGE_EXP_QTREWARDEXPFROMROLELEVEL			= 31	# 交任务获得经验
CHANGE_EXP_QTREWARDSECONDPERCENTEXP			= 32	# 交任务获得经验
CHANGE_EXP_QTREWARDEXPFROMTABLE				= 33	# 交任务获得经验
CHANGE_EXP_AIACTION							= 34	# 获取家族NPC发送经验
CHANGE_EXP_FAMILYTEAM						= 35	# 家族组队获取额外经验
CHANGE_EXP_NORMAL							= 36	# 普通的改变经验,如杀怪等
CHANGE_EXP_CITYWAR_OVER						= 37	# 城战结束给与奖励
CHANGE_EXP_ROBOT_VERIFY_RIGHT				= 38	# 反外挂验证答题奖励
CHANGE_EXP_CMS_SOCKS						= 39	# 圣诞袜子
CHANGE_EXP_OLD_PLAYER_REWARD				= 40	# 老玩家计时获取经验
CHANGE_EXP_YYD_BOX							= 41	# 打开荣誉宝盒
CHANGE_EXP_AND_POTENTIAL					= 42	# 经验换潜能
CHANGE_EXP_TONG_ROB							= 43	# 掠夺战给的奖励
CHANGE_EXP_FISHING							= 44	# 钓鱼的奖励
CHANGE_EXP_TEACH_BUFF						= 45	# 师徒共勉buff的奖励
CHANGE_EXP_RABBITRUN						= 46	# 小兔快跑
CHANGE_EXP_TONG_DART_ROB					= 47	# 镖车刷劫匪时，给与玩家经验奖励
CHANGE_EXP_TEAM_CHALLENGE					= 48	# 组队擂台奖励的经验
CHANGE_EXP_USE_ROLE_COMPETITION_ITEM		= 49	#使用个人竞技经验丹
CHANGE_EXP_CITYWAR_MASTER					= 50	# 城主给予奖励
CHANGE_EXP_TONGCOMPETITION					= 51	# 帮会竞技经验奖励
CHANGE_EXP_TONGCOMPETITION_BOX				= 52	# 帮会竞技箱子经验奖励

#----------------------------------------------------------------------
#点卡寄售(充值处理)
#-----------------------------------------------------------------------
RECHANGE_OVERTIME_FAILED					= -1		# 超时错误
RECHANGE_SUCCESS							= 0			# 点卡寄售成功
RECHANGE_NO_OR_PWD_FAILED				    = 1			# 卡号或密码错误
RECHANGE_USED_FAILED					    = 2			# 已使用的卡
RECHANGE_ACCOUNT_NOT_ACTIVITIED_FAILED	    = 3			# 帐号未在该区激活
RECHANGE_ACCOUNT_NOT_HAVE_FAILED		    = 4			# 帐号不存在
RECHANGE_FAILED							    = 5			# 充值失败
RECHANGE_MD5_FAILED						    = 6			# MD5校验失败
RECHANGE_PARAMS_FAILED					    = 7			# 参数不完整
RECHANGE_SERVER_NAME_FAILED				    = 8			# 不存在的服务器名
RECHANGE_OVER_DUPLICATE_FAILED			    = 9			# 定单号重复
RECHANGE_TEN_YUAN						    = 10		# 10元面值的卡
RECHANGE_IP_FAILED						    = 11		# IP错误，服务器中文名和服务器的IP对应不上
RECHANGE_ACCOUNT_MSG_FALIED				    = 12		# 获取帐号信息失败
RECHANGE_CARD_LOCKED_CARD				    = 13		# 已封号的卡
RECHANGE_LOGGED_FALID					    = 14		# 写入充值日志失败
RECHANGE_CARD_NOT_EXIST_CARD			    = 15		# 卡不存在 或 卡未激活
RECHANGE_SEND_YUANBAO_FAILED			    = 16		# 操作成功，但是发放元宝失败
RECHANGE_THIRTY							    = 30		# 30元面值的卡
RECHANGE_CARD_VALUE_FAILED				    = 17		# 面值不符


#----------------------------------------------------------------------
#点卡寄售(超时处理)
#----------------------------------------------------------------------
OVERTIME_OVERTIME_FAILED					= -1		# 超时错误
OVERTIME_RECHANGE_SUCCESS					= 0	    	# 超时检测充值成功
OVERTIME_RECHANGE_FAILED					= 1	    	# 超时检测充值错误
OVERTIME_NO_ORDER_FAILED					= 2 		# 超时没有这个订单


ROLE_RELATION_BLACKLIST						= 0x0001		# 玩家黑名单关系
ROLE_RELATION_FOE								= 0x0002		# 玩家仇人关系
ROLE_RELATION_MASTER						= 0x0004		# 玩家师父关系
ROLE_RELATION_PRENTICE						= 0x0008		# 玩家徒弟关系
ROLE_RELATION_FRIEND							= 0x0010		# 玩家好友关系
ROLE_RELATION_SWEETIE						= 0x0020		# 玩家恋人关系
ROLE_RELATION_COUPLE							= 0x0040		# 玩家夫妻关系
ROLE_RELATION_ALLY							= 0x0080		# 玩家结拜关系
ROLE_RELATION_MASTER_EVER					= 0x0100		# 玩家过去的师父
ROLE_RELATION_PRENTICE_EVER				= 0x0200		# 玩家过去的徒弟


#-----------------------------------------------------------------------
# 骑宠/角色动作标记（音效用） by姜毅
#-----------------------------------------------------------------------
VEHICLE_ACTION_TYPE_CONJURE		= 1		# 召唤
VEHICLE_ACTION_TYPE_JUMP		= 2		# 跳跃
VEHICLE_ACTION_TYPE_RANDOM		= 3		# 随机动作
VEHICLE_ACTION_TYPE_WALK		= 4		# 行走

#-----------------------------------------------------------------------
# 切磋状态
#-----------------------------------------------------------------------
QIECUO_NONE						= 0		# 切磋默认状态
QIECUO_INVITE					= 1		# 切磋邀请状态
QIECUO_BEINVITE					= 2		# 切磋被邀请状态
QIECUO_READY					= 3		# 切磋准备状态
QIECUO_FIRE						= 4		# 切磋进行状态



#------------------------------------------------------------------------
#增加宠物的原因
#------------------------------------------------------------------------
ADDPET_INIT						= 1		# 初始化
ADDPET_PETTRADING				= 2		# 宠物交易
ADDPET_TAKEPETFROMBANK			= 3		# 仓库中取出宠物
ADDPET_CATCHPET					= 4		# 捕捉宠物
ADDPET_FOSTER					= 5		# 宠物繁殖
ADDPET_RECEIVETSPET				= 6		# 获取寄售的宠物
ADDPET_BUYFROMVEND				= 7		# 从摆摊中购买


#------------------------------------------------------------------------
#删除宠物的原因
#------------------------------------------------------------------------
DELETEPET_PETTRADING			= 1		# 宠物交易
DELETEPET_PETSTORE				= 2		# 仓库中存储宠物
DELETEPET_FREEPET				= 3		# 放生宠物
DELETEPET_COMBINEPETS			= 4		# 宠物合成
DELETEPET_PROCREATEPET			= 5		# 宠物繁殖
DELETEPET_TSPET					= 6		# 出售寄售宠物
DELETEPET_VEND_SELLPET			= 7		# 摆摊出售


#-------------------------------------------------------------------------
#银元宝改变的原因
#-------------------------------------------------------------------------
CHANGE_SILVER_WEEKONLINETIMEGIFT	=	1	# 领取周奖励
CHANGE_SILVER_TESTWEEKGIFT			=	2	# 领取周奖励(老接口)
CHANGE_SILVER_SILVERPRESENT			=	3	# 领取银元宝奖励
CHANGE_SILVER_CHARGE				=	4	# 充值兑换银元宝
CHANGE_SILVER_GMCOMMAND				=	5	# GM命令
CHANGE_SILVER_BUYITEM				=	6	# 购买商城物品
AUTOUSEYELL							=	7	# 自动购买物品
CHANGE_SILVER_ITEMTOSILVER			=	8	# 元宝票变成银元宝
CHANGE_SILVER_MESSY_TAKE			=	9	# 每天领取（一个活动）

#-------------------------------------------------------------------------
#金元宝改变的原因
#-------------------------------------------------------------------------
CHANGE_GOLD_BANK_ITEM2GOLD			=	1	# 元宝票兑换成金元宝
CHANGE_GOLD_GEM_CHARGE				=	2	# 充值代练宝石
CHANGE_GOLD_PTN_TRAINCHARGE			=	3	# 充值宠物代练宝石
CHANGE_GOLD_QUIZ_USEGOLD			=	4	# 使用元宝答题
CHANGE_GOLD_BUYITEM					=	5	# 购买商城物品
CHANGE_GOLD_PST_HIRE				=	6	# 租用宠物仓库
AUTOUSEYELL							=	7	# 自动购买物品
CHANGE_GOLD_BANK_CHANGEGOLDTOITEM	=	8	# 元宝票兑换
CHANGE_GOLD_CHARGE					=	9	# 充值兑换
CHANGE_GOLD_GMCOMMAND				=	10	# GM命令
CHANGE_GOLD_YBT_CANCLE_BILL				=	11	# 元宝交易撤销订单 by姜毅
CHANGE_GOLD_YBT_SELL						=	12	# 元宝交易卖元宝 by姜毅
CHANGE_GOLD_YBT_SELL_BILL					=	13	# 元宝交易建立寄售订单 by 姜毅
CHANGE_GOLD_YBT_DRAW_YB					=	14	# 取出账号元宝 by 姜毅


# 与npc交易商品价格类型
INVOICE_NEED_MONEY = 0x01				# 钱物交易
INVOICE_NEED_ITEM = 0x02					# 物物交易
INVOICE_NEED_DANCE_POINT = 0x04		# 跳舞积分换物品
INVOICE_NEED_TONG_CONTRIBUTE	= 0x08	# 帮会贡献
INVOICE_NEED_CHEQUE = 0x10				# 银票交易
INVOICE_NEED_ROLE_PERSONAL_SCORE = 0x20				# 个人竞技积分
INVOICE_NEDD_TEAM_COMPETITION_POINT		= 0x40	#组队竞技积分
INVOICE_NEED_TONG_SCORE   = 0x80		# 帮会竞技积分换物品
INVOICE_NEED_ROLE_ACCUM_POINT = 0x100	# 气运值换物品
INVOICE_NEED_SOUL_COIN = 0x200	# 灵魂币换物品

# npc商品类型
INVOICE_CLASS_TYPE_NORMAL	= 1			# 普通商品
INVOICE_CLASS_TYPE_BIND		= 2			# 销售后绑定



#-------------------------------------------------------------------------
# 生活系统 by 姜毅
#-------------------------------------------------------------------------
LIVING_SKILL_MAX	=	1	# 角色允许学习的生活技能数上限

#-------------------------------------------------------------------------
# 奖励配置 by 姜毅
# 必备前缀:RCG_	(reward config)
#-------------------------------------------------------------------------

RCG_OLD_PLAYER_FIXTIME		=	10001			# 老玩家在线奖励
RCG_TONG_FETE					= 10111		# 帮会祭祀奖励

RCG_VIP_PACK					= 10021	# VIP礼包
RCG_LV_20_PACK				= 10022	# 20级礼包
RCG_LV_30_PACK				= 10023	# 30级礼包
RCG_LV_40_PACK				= 10024	# 40级礼包
RCG_LV_50_PACK				= 10025	# 50级礼包
RCG_NEWCARD_LV_15_PACK	= 10026	# 15级新手卡礼包
RCG_NEWCARD_LV_35_PACK	= 10027	# 35级新手卡礼包
RCG_CHINA_JOY_PACK		= 10028	# CHINA JOY礼包
RCG_TUIGUANG_PACK		= 10029	# 推广员礼包
RCG_BENQ_PACK			= 10030	# 明基礼包
RCG_NORMAL_CASSTO_PACK	= 10031	# 普通魂魄石礼包
RCG_INST_STUF_PACK		= 10032	# 装备强化材料礼包
RCG_EQUMAKE_STUF_PACK	= 10033	# 装备打造材料礼包
RCG_PERF_CASSTO_PACK	= 10034	# 完美魂魄石大礼包
RCG_LV10_PACK			= 10035	# 10级礼包
RCG_NEWCARD_LV_5_PACK	= 10036	# 5级新手卡礼包
RCG_NEWCARD_LV_25_PACK	= 10037	# 25级新手卡礼包
RCG_NEWCARD_LV_45_PACK	= 10038	# 45级新手卡礼包

RCG_FISH						= 10301	# 钓鱼
RCG_KJ							= 10302	# 科举奖励
RCG_HAPPY_GODEN_EGG		= 10303	# 快乐金蛋
RCG_SUPER_HAPPY_GODEN_EGG	= 10304	# 超级快乐金蛋
RCG_HAPPY_GODEN_EGG_V	= 10305	# 快乐金蛋骑宠奖励
RCG_WD_FORE_LEVEL			= 10306	# 武道大会4lv奖励
RCG_WD_THREE_LEVEL			= 10307	# 武道大会3lv奖励
RCG_SPRING_LIGHT			= 10308	# 春节灯谜
RCG_TONG_CITY_EXP			= 10309	# 城市占领帮会的经验果实
RCG_TONG_ROB_WAR_1		= 10400	# 掠夺战奖励领取 1
RCG_TONG_ROB_WAR_2		= 10401	# 掠夺战奖励领取 2
RCG_TONG_ROB_WAR_3		= 10402	# 掠夺战奖励领取 3
RCG_D_TEACH					= 10403	# 出师奖励的物品列表
RCG_MARRY_RING				= 10404	# 结婚戒指物品
RCG_TEACH_END_SUC			= 10405	# 徒弟出师成功所获得的出师状
RCG_TEACH_END_SUC_THK	= 10406	# 徒弟出师成功师父所获得的感恩状
RCG_TEACH_LEVEL_UP			= 10407	# 徒弟升级获得的勉励状
RCG_TEACH_SUC				= 10408	# 拜师成功
RCG_TEACH_EVERY_DAY		= 10409	# 徒弟每日师徒奖励小型经验丹
RCG_TANABATA_FRUIT		= 10411	# 七夕活动魅力果实奖励
RCG_MID_AUT_YGBH			= 10412	# 中秋活动月光宝盒
RCG_QUIZ_GAME_FESTIVAL		= 10413	# 知识问答特定节日奖励

RCG_RACE_HORSE				= 10500	# 赛马物品奖励

RCG_TEAM_COMP_EXP_1		= 10501	# 组队竞赛经验奖励1
RCG_TEAM_COMP_EXP_2		= 10502	# 组队竞赛经验奖励2
RCG_TEAM_COMP_EXP_3		= 10503	# 组队竞赛经验奖励3
RCG_TEAM_COMP_EXP_4		= 10504	# 组队竞赛经验奖励4
RCG_TEAM_COMP_EXP_5		= 10505	# 组队竞赛经验奖励5
RCG_TEAM_COMP_EXP_6		= 10506	# 组队竞赛经验奖励6
RCG_TEAM_COMP_EXP_7		= 10507	# 组队竞赛经验奖励7
RCG_TEAM_COMP_EXP_8		= 10508	# 组队竞赛经验奖励8
RCG_TEAM_COMP_EXP_9		= 10509	# 组队竞赛经验奖励9
RCG_TEAM_COMP_EXP_10		= 10510	# 组队竞赛经验奖励10

RCG_LUCKY_BOX		= 10511	# 天降宝盒
RCG_MID_AUTUMN	= 10512	# 中秋掉落

RCG_RACE_HORSE_CHRIS			= 10513	# 圣诞赛马物品奖励
RCG_TONG_ABA					= 10514	# 帮会擂台赛冠军物品奖励,id还未确定

#-------------------------------------------------------------------------
#活动主类型
#-------------------------------------------------------------------------
ACTIVITY_PARENT_TYPE_QUEST				= 0				#任务类
ACTIVITY_PARENT_TYPE_SPACECOPY			= 1				#副本类
ACTIVITY_PARENT_TYPE_OTHER				= 2				#其他类
ACTIVITY_PARENT_TYPE_MONSTER_DIED		= 3				#怪物死亡
ACTIVITY_PARENT_TYPE_CHANGE_MONSTER		= 4				#变成怪物


#-------------------------------------------------------------------------
#活动类型
#-------------------------------------------------------------------------
ACTIVITY_ROLE_UP					=	-1	#角色上线
ACTIVITY_LOOP_QUEST_30_59			=	0	#30-59级环任务
ACTIVITY_LOOP_QUEST_60_95			=	1	#60-95级环任务
ACTIVITY_TONG_LUE_DUO				=	2	#帮会掠夺战
ACTIVITY_TONG_DUO_CHENG				=	3	#帮会夺城战
ACTIVITY_NPC_LUE_DUO				=	4	#NPC掠夺战
ACTIVITY_NPC_GUA_JING_YAN			=	5	#NPC挂经验
ACTIVITY_JIA_ZU_LEI_TAI				=	6	#家族擂台赛
ACTIVITY_JIA_ZU_TIAO_ZHAN			=	7	#家族挑战赛
ACTIVITY_XING_XIU					=	8	#星宿挑战
ACTIVITY_TAO_FA						=	9	#讨伐任务
ACTIVITY_NORMAL_DART				=	10	#普通镖任务
ACTIVITY_EXP_DART					=	11	#贵重镖任务
ACTIVITY_FAMILY_DART				=	12	#家族镖任务
ACTIVITY_CAI_LIAO_QUEST				=	13	#材料任务
ACTIVITY_KE_JU_QUEST				=	14	#科举答题
ACTIVITY_JIAO_FEI_QUEST				=	15	#剿匪任务
ACTIVITY_CHU_YAO_QUEST				=	16	#除妖任务
ACTIVITY_SHANG_HUI_QUEST			=	17	#商会任务
ACTIVITY_SHENG_WANG					=	18	#声望任务
ACTIVITY_FAMILY_RICHANG_QUEST		=	19	#家族日常任务
ACTIVITY_TONG_JISHI_QUEST			=	20	#帮会祭祀
ACTIVITY_TONG_JIANSHE_QUEST			=	21	#帮会建设任务
ACTIVITY_TONG_RICHANG_QUEST			=	22	#帮会日常任务
ACTIVITY_TONG_MERCHANT				=	23	#帮会跑商任务
ACTIVITY_CHUANG_TIAN_GUAN			=	24	#闯天关
ACTIVITY_CANG_BAO_TU				=	25	#藏宝图
ACTIVITY_LING_GONG_ZI				=	26	#领工资
ACTIVITY_TONG_PROTECT				=	27	#保护帮派
ACTIVITY_BIN_LIN_CHENG_XIA			=	28	#兵临城下
ACTIVITY_SAI_MA						=	29	#赛马
ACTIVITY_DU_DU_ZHU					=	30	#嘟嘟猪
ACTIVITY_FENG_JIAN_SHEN_GONG		=	31	#封剑神宫
ACTIVITY_GE_REN_JING_JI				=	32	#个人竞技
ACTIVITY_TOU_JI_SHANG_REN			=	33	#投机商人
ACTIVITY_HUN_DUN_RU_QIN				=	34	#混沌入侵
ACTIVITY_NIU_MO_WANG				=	35	#牛魔王
ACTIVITY_QIAN_NIAN_DU_WA			=	36	#千年毒蛙
ACTIVITY_BANG_HUI_JING_JI			=	37	#帮会竞技
ACTIVITY_TIAO_WU					=	38	#跳舞
ACTIVITY_BIAN_SHEN_DA_SAI			=	39	#变身大赛
ACTIVITY_DIAO_YU					=	40	#钓鱼
ACTIVITY_CAI_JI_ZHEN_ZHU			=	41	#采集珍珠
ACTIVITY_JING_YAN_LUAN_DOU			=	42	#经验乱斗
ACTIVITY_QIAN_NENG_LUAN_DOU			=	43	#潜能乱斗
ACTIVITY_SHE_HUN_MI_ZHEN			=	44	#摄魂迷阵
ACTIVITY_SHEN_GUI_MI_JING			=	45	#神鬼秘境
ACTIVITY_SHI_LUO_BAO_ZHANG			=	46	#失落宝藏
ACTIVITY_WU_YAO_QIAN_SHAO			=	47	#巫妖前哨
ACTIVITY_SHUI_JING					=	48	#水晶副本
ACTIVITY_TIAN_CI_QI_FU				=	49	#天赐祈福
ACTIVITY_TIAN_JIANG_QI_SHOU			=	50	#天降奇兽
ACTIVITY_ZHI_SHI_WEN_DA				=	51	#知识问答
ACTIVITY_WU_DAO_DA_HUI				=	52	#武道大会
ACTIVITY_ZHENG_JIU_YA_YU			=	53	#拯救猰貐
ACTIVITY_ZU_DUI_LUAN_DOU			=	54	#组队乱斗
ACTIVITY_DUO_LUO_LIE_REN			=	55	#堕落猎人
ACTIVITY_BAI_SHE_YAO				=	56	#白蛇妖
ACTIVITY_JU_LING_MO					=	57	#巨灵魔
ACTIVITY_XIAO_TIAN_DA_JIANG			=	58	#啸天大将
ACTIVITY_FENG_KUANG_JI_SHI			=	59	#疯狂祭师
ACTIVITY_HAN_DI_DA_JIANG			=	60	#憾地大将
ACTIVITY_SHENG_LIN_ZHI_WANG			=	61	#森林之主
ACTIVITY_NU_MU_LUO_SHA				=	62	#怒目罗刹
ACTIVITY_YE_WAI_BOSS				=	63	#野外BOSS
ACTIVITY_NPC_ZHENG_DUO				=	64	#NPC争夺战
ACTIVITY_BAO_XIANG					=	65	#天降宝盒（宝箱）
ACTIVITY_FAMILY_NPC					=	66	#拥有NPC的家族数量
ACTIVITY_FAMILY_COUNT				=   67  #家族数量
ACTIVITY_TONG_COUNT					=   68  #帮会数量
ACTIVITY_QUEST_SHOUJI				=   69  #收集任务
ACTIVITY_QUEST_LAN_LV				=   70  #蓝装换绿装
ACTIVITY_SHI_TU						=	71  #师徒活动
ACTIVITY_NPC_BUY					=	72  #NPC购买玩家物品
ACTIVITY_TONG_GONGZI				=	73  #帮会发工资活动
ACTIVITY_NPC_SELL					=	74  #玩家购买NPC物品
ACTIVITY_XIU_LI						=	75  #玩家修理装备
ACTIVITY_SHEN_JI_XIA				=	76  #神机匣活动
ACTIVITY_QUEST_JU_QING				=	77  #剧情任务
ACTIVITY_RUN_RABBIT					=   78  #小兔快跑
ACTIVITY_BANG_HUI_LEI_TAI			=	79	#帮会擂台赛
ACTIVITY_XIE_LONG					= 	80  #邪龙洞穴
ACTIVITY_POTENTIAL					= 	81  #潜能副本（剿匪，除妖等）
ACTIVITY_ROLE_VEND					=	82	#摆摊
ACTIVITY_ROLE_TI_SHOU				=	83	#寄售
ACTIVITY_ROLE_EXP_GEM				=	84	#角色代练
ACTIVITY_PET_EXP_GEM				=	85	#宠物代练
ACTIVITY_TALISMAN					= 	86  #法宝
ACTIVITY_EQUIP						= 	87  #装备相关
ACTIVITY_CMS_SOCKS					= 	88	#圣诞袜子
ACTIVITY_MEMBER_DART				=	89	#成员运镖
ACTIVITY_SPRING_RIDDER				=	90	#春季赛马
ACTIVITY_FEI_CHENG_WU_YAO			= 	91	#非诚勿扰
ACTIVITY_TANABATA_QUIZ				=	92	#七夕问答
ACTIVITY_KUA_FU						=	93	#夸父神殿
ACTIVITY_TONG_FUBEN					= 	94	#帮会副本
ACTIVITY_CHALLENGE_FUBEN			= 	95	#单人挑战副本
ACTIVITY_CHALLENGE_FUBEN_MANY		=	96	#三人挑战副本
ACTIVITY_YING_XIONG_LIAN_MENG		=	97	#英雄联盟副本




DIRECT_WRITE_LOG_MGR				= 	0	#活动统计直接写日志方式
WRITE_BUFFER_LOG_MGR				= 	1	#活动统计缓冲写日志方式




#任务活动行为
ACTIVITY_QUEST_ACTION_ACCEPT		=	0	#接受任务
ACTIVITY_QUEST_ACTION_COMPLETE		=	1	#完成任务
ACTIVITY_QUEST_ACTION_ABANDON		=	2	#放弃任务

#竞技类活动行为
ACTIVITY_COMPETITION_START			= 	0	#开启
ACTIVITY_COMPETITION_ROLE_JOIN		= 	1	#个人参与
ACTIVITY_COMPETITION_GROUP_JOIN		= 	2	#团体参与（队伍，家族，帮会）
ACTIVITY_COMPETITION_TEAM_POINT		=	3	#积分
ACTIVITY_COMPETITION_ADD_HONOR		=	4	#增加荣誉
ACTIVITY_COMPETITION_SUB_HONOR		=	5	#减少荣誉
ACTIVITY_COMPETITION_CHANGE_BOX		=	6	#兑换宝箱
ACTIVITY_COMPETITION_SIGN_UP		=	7	#报名/竞标
ACTIVITY_COMPETITION_ROLE_KILL		=	8	#杀死玩家
ACTIVITY_COMPETITION_TIME			=	9	#开启时间
ACTIVITY_COMPETITION_ADD_PERSONAL_SCORE	=	10	#增加个人竞技积分
ACTIVITY_COMPETITION_SUB_PERSONAL_SCORE	=	11	#减少个人竞技积分

#杂action
ACTIVITY_ACTION_ROLE_UP			= 	0	#角色上线
ACTIVITY_ACTION_ROLE_ADD_EXP	= 	1	#角色获得经验
ACTIVITY_ACTION_ROLE_TRIGGER	= 	2	#触发（开启怪物攻城类怪物）
ACTIVITY_ACTION_MONSTER_DIED	= 	3	#怪物死亡
ACTIVITY_ACTION_COPY_START		= 	4	#集体活动开启
ACTIVITY_ACTION_USE_ITEM		= 	5	#使用物品
ACTIVITY_ACTION_LING_GONG_ZI	= 	6	#领取工资
ACTIVITY_ACTION_TRADE			= 	7	#交易
ACTIVITY_ACTION_DANCE			= 	8	#跳舞
ACTIVITY_ACTION_BODY_CHANGE		= 	9	#变身
ACTIVITY_ACTION_PASSED			= 	10	#通过
ACTIVITY_ACTION_DIAO_YU			= 	11	#钓鱼
ACTIVITY_ACTION_CAIJI			=	12	#采集
ACTIVITY_ACTION_QI_FU			=	13	#祈福
ACTIVITY_ACTION_JI_SI			=	14	#祭祀
ACTIVITY_ACTION_REWARD_MONEY	= 	15	#奖励金钱
ACTIVITY_ACTION_TONG_CITY_RECORD	= 	16	#城战记录
ACTIVITY_ACTION_REWARD_MONEY_CONTRIBUTE = 17	# 奖励帮会贡献
ACTIVITY_ACTION_CITY_MASTER_SET	= 18	# 城主设置
ACTIVITY_ACTION_CITY_WAR_SIGN_UP_COUNT	= 19	# 城战竞拍总数
ACTIVITY_ACTION_COPY_SPACE_LEVEL	= 20 #副本等级段
ACTIVITY_ACTION_CURRENT_PLAYER_COUNT = 21 #当前玩家数
ACTIVITY_ACTION_ROLE_START_BEHAVIOR		 = 22 #开启一项行为（替售，摆摊，角色代练，宠物代练等）
ACTIVITY_ACTION_TALISMAN_REBUILD	= 23 #法宝洗属性
ACTIVITY_MAKE_EQUIP					= 24 #装备打造（装备锻造）
ACTIVITY_ACTION_BUY_ITEM			= 25 #买物品
ACTIVITY_ACTION_BUY_PET				= 26 #买宠物
ACTIVITY_ACTION_REWARD_TONG_ACITONVAL = 27	# 奖励帮会行动力
ACTIVITY_ACTION_REWARD_DAOHENG		= 28 # 奖励道行

# ----------------------------------------------------
# 元宝交易相关
# ----------------------------------------------------
YB_TRADE_TYPE_BUY		= 1		# 交易行为 购买元宝
YB_TRADE_TYPE_SELL		= 2		# 交易行为 售卖元宝
YB_BILL_TYPE_BUY			= 1		# 订单类型 求购元宝
YB_BILL_TYPE_SELL			= 2		# 订单类型 寄售元宝
YB_BILL_STATE_FREE			= 0		# 订单状态 自由
YB_BILL_STATE_TRADE_LOCK = 1		# 订单状态 交易锁定
YB_BILL_STATE_OVER_DUE	= 2		# 订单状态 过期
YB_BILL_STATE_SELL_OUT	= 3		# 订单状态 售空
YB_BILL_STATE_ON_TRADE	= 4		# 订单状态 处理中
YB_RECORD_BUY_BILL		= 1		# 明细类型 创建寄售订单
YB_RECORD_SELL_BILL		= 2		# 明细类型 创建求购订单
YB_RECORD_BUY			= 3		# 明细类型 买元宝
YB_RECORD_SELL			= 4		# 明细类型 卖元宝


# ----------------------------------------------------
# 怪物攻城
# ----------------------------------------------------
SPECIAL_BOX_01	= 1							#迷茫之箱
SPECIAL_BOX_02	= 2							#恐惧之箱
SPECIAL_BOX_03  = 3							#黑暗之箱


# ----------------------------------------------------
# 杂乱的事情管理（主要是一些记录玩家做过某些事情的信息）
# ----------------------------------------------------
MESSY_JOB_TAKE_SILVER	= 1					#领取元宝


#------------------------------------------------------
#非诚勿扰
#------------------------------------------------------
FCWR_VOTE_ALL			= 0					#总投票
FCWR_VOTE_KAN_HAO		= 1					#看好
FCWR_VOTE_QING_DI		= 2					#情敌
FCWR_VOTE_SHI_LIAN		= 3					#失恋
FCWR_VOTE_KAN_QI		= 4					#看好戏
FCWR_VOTE_FAN_DUI		= 5					#反对
FCWR_VOTE_LU_GUO		= 6					#路过

FCWR_MAX_COUNT_VOTER_1		= 7					#投票数目第一者
FCWR_MAX_COUNT_VOTER_2		= 8					#投票数目第二者
FCWR_MAX_COUNT_VOTER_3		= 9					#投票数目第三者

#------------------------------------------------------
# 魅力果树成熟方式
#------------------------------------------------------
FRUIT_TREE_RIPE_NORMAL	 = 1		# 果树自然成熟
FRUIT_TREE_RIPE_FAST	 = 2		# 果树施肥成熟

# ----------------------------------------------------------------------------
# 保护帮派活动类型
# ----------------------------------------------------------------------------
PROTECT_TONG_NORMAL			= 0		# 保护帮派普通活动
PROTECT_TONG_MID_AUTUMN		= 1		# 保护帮派中秋活动

# ----------------------------------------------------------------------------
# 帮会活动魔物来袭 added by mushuang
# ----------------------------------------------------------------------------
MONSTER_RAID_TIME_OUT_CBID = 100 # 活动时间到的callback id
PRIZE_DURATION = 15 * 60 # 宝箱存在的时间

#-----------------------------------------------------------------------------
#帮会祭祀完成之后帮会可以获得的状态 参见 CSOL-9750 by mushuang
#-----------------------------------------------------------------------------
NONE_STATUS = 0
LUNARHALO = 1	# 月晕甘露
SUNSHINE = 2	# 日光甘露
STARLIGHT = 3	# 星辉甘露



TONG_FETE_ACT_VAL = 30 # 帮会祭祀所需的行动力

FLYING_BUFF_ID = 8006  # 飞行骑宠buff的ID
VEHICLE_BUFF_ID = 6005 # 陆地骑宠buff的ID

#-----------------------------------------------------------------------------
#购买替售物品（主要用于 活动日志）
#-----------------------------------------------------------------------------
TISHOU_BUY_FROM_TISHOUMGR	= 0				#通过管理器购买
TISHOU_BUY_FROM_TISHOUNPC	= 1				#通过NPC购买
TISHOU_BUY_SUCCESS			= 2				#购买成功

ROLE_DIE_DROP_PROTECT_LEVEL = 30			# 玩家死亡掉落保护级别，小于此级别不掉落装备、物品

#-----------------------------------------------------------------------------
#玩家vip级别定义
#-----------------------------------------------------------------------------
ROLE_VIP_LEVEL_NONE			= 0
ROLE_VIP_LEVEL_GOLD			= 1
ROLE_VIP_LEVEL_PLATINUM		= 2
ROLE_VIP_LEVEL_DIAMOND		= 3

#世界摄像机控制方式
NORMAL_WORLD_CAM_HANDLER		= 1	#常规摄像机控制方式
FIX_WORLD_CAM_HANDLER			= 2	#固定视角摄像机控制方式

#-----------------------------------------------------------------------------
# 跳跃定义
#-----------------------------------------------------------------------------
JUMP_TIME_UP1					= 0x1				# 跳跃上升阶段1
JUMP_TIME_UP2					= 0x2				# 跳跃上升阶段2
JUMP_TIME_UP3					= 0x3				# 跳跃上升阶段3
JUMP_TIME_DOWN					= 0x4				# 跳跃下落阶段
JUMP_TIME_END					= 0x5				# 跳跃结束阶段
JUMP_TIME_UPPREPARE				= 0x6				# 空中蓄力阶段

JUMP_TYPE_LAND					= 0x10				# 陆地跳跃
JUMP_TYPE_WATER1				= 0x20				# 水面跳跃1
JUMP_TYPE_WATER2				= 0x30				# 水面跳跃2
JUMP_TYPE_WATER3				= 0x40				# 水面跳跃3
JUMP_TYPE_ATTACK				= 0x80				# 跳砍

JUMP_TIME_MASK					= 0x0000000f		# 跳跃阶段掩码
JUMP_TYPE_MASK					= 0x000000f0		# 跳跃类型掩码



JUMP_UP2_SKILLID                                = 322537001 #2段跳技能ID
JUMP_UP3_SKILLID                                = 322537002 #3段跳技能ID

JUMP_UP2_ENERGY                                = 5   #2段跳消耗能量值
JUMP_UP3_ENERGY                                = 10  #3段跳消耗能量值
JUMP_ENERGY_MAX                                = 100 #跳跃能量最大值

ROLE_ENERGY_REVER_INTERVAL                      = 1.0  #跳跃能量值的自动恢复timer
ROLE_ENERGY_REVER_VALUE                         = 5.0  #跳跃能量值的自动恢复值


#-----------------------------------------------------------------------------
# 象服务器请求分类
#-----------------------------------------------------------------------------
ROLE_INITIALIZE					= 0					# 角色初始化类型


#-----------------------------------------------------------------------------
# 挑战副本
#-----------------------------------------------------------------------------
CHALLENGE_RANK_EASY = 0
CHALLENGE_RANK_HARD = 1


MATCH_LEVEL_NONE = 0			#
MATCH_LEVEL_FINAL = 1			# 决赛
MATCH_LEVEL_SEMIFINALS = 2		# 半决赛
MATCH_LEVEL_QUARTERFINAL = 3	# 四分之一决赛
MATCH_LEVEL_QUARTERFINAL = 4	# 八分之一决赛
MATCH_LEVEL_SIXTEENTH = 5		# 十六分之一决赛
MATCH_LEVEL_SIXTEENTH = 6		# 三十二分之一决赛

MATCH_TYPE_PERSON_ABA = 1			# 武道大会
MATCH_TYPE_TEAM_ABA = 2				# 组队擂台
MATCH_TYPE_TONG_ABA = 3				# 帮会擂台
MATCH_TYPE_TEAM_COMPETITION = 4		# 队伍竞技
MATCH_TYPE_TONG_COMPETITION = 5		# 帮会竞技
MATCH_TYPE_PERSON_COMPETITION = 6	# 个人竞技

#-----------------------------------------------------------------------------
# 消耗统计(父类)
#-----------------------------------------------------------------------------
CPU_COST_SKILL				= 1		#技能消耗
CPU_COST_AI					= 2		#AI消耗



#-----------------------------------------------------------------------------
# 消耗统计(子类)
#-----------------------------------------------------------------------------
CPU_COST_SKILL_CHECK		= 1	#检测技能
CPU_COST_SKILL_USE			= 2	#使用技能
CPU_COST_SKILL_ARRIVE		= 3 #技能到达


CPU_COST_AI_FREE		= 1 #ai 自由
CPU_COST_AI_FIGHT		= 2 #ai 战斗


#-----------------------------------------------------------------------------
# 活动参与状态
#-----------------------------------------------------------------------------
ACTIVITY_CAN_JOIN					= 1	#可以参与
ACTIVITY_CAN_NOT_JOIN				= 2 #不能参与


#-----------------------------------------------------------------------------
# 副本组队系统
#-----------------------------------------------------------------------------
COPY_DUTY_MT			= 1					# 副本队伍职责坦克
COPY_DUTY_HEALER		= 2					# 副本队伍职责治疗
COPY_DUTY_DPS			= 3					# 副本队伍职责伤害输出



TIME_LIMIT_OF_DUTIES_SELECTION 	= 120.0		# 职责选择时间限制，单位：秒
TIME_LIMIT_OF_MATCHED_CONFIRM 	= 120.0		# 匹配确认时间限制，单位：秒
TIME_LIMIT_OF_CONFIRM_RESUMING 	= 120.0		# 确认加入半路副本队伍的时间限制，单位：秒
TIME_LIMIT_OF_MATCHED_INTERVAL	= 900.0		# 匹配器再次使用的冷却时间， 单位：秒
TIME_LIMIT_OF_KICKING_INTERVAL	= 300.0		# 发起剔除队友投票的时间间隔，单位：秒
# 队伍职责匹配状态定义
COPY_TEAM_DUTIES_CONFLICT 				= 1			# 职责冲突
COPY_TEAM_DUTIES_MATCH_PARTIALLY 		= 2			# 职责部分匹配，队伍还缺其他职责
COPY_TEAM_DUTIES_MATCH_PLENARILY 		= 3			# 职责完全匹配，队伍匹配成功
COPY_TEAM_BLACKLIST_CONFLICT			= 4			# 黑名单冲突
COPY_TEAM_RECRUITER_CONFLICT			= 5			# 招募者冲突
# 个人匹配状态
MATCH_STATUS_PERSONAL_NORMAL			= 0			# 个人匹配状态：普通状态
MATCH_STATUS_PERSONAL_SELECTING_DUTY	= 1			# 个人匹配状态：职责选择状态(组队玩家才会有这种状态)
MATCH_STATUS_PERSONAL_MATCHING			= 2			# 个人匹配状态：正在匹配状态
MATCH_STATUS_PERSONAL_CONFIRMING		= 3			# 个人匹配状态：正在职责确认
MATCH_STATUS_PERSONAL_MATCHED			= 4			# 个人匹配状态：职责确认成功
# 匹配成功状态
MATCH_STATUS_PERSONAL_INSIDECOPY		= 0			# 个人匹配状态：匹配成功，在副本内
MATCH_STATUS_PERSONAL_OUTSIDECOPY		= 1			# 个人匹配状态：匹配成功，在副本外
# 队伍匹配状态
MATCH_STATUS_TEAM_NORMAL				= 0			# 队伍匹配状态：普通队伍
MATCH_STATUS_TEAM_SELECTING_DUTY		= 1			# 队伍匹配状态：职责选择状态(组队玩家才会有这种状态)
MATCH_STATUS_TEAM_MATCHING				= 2			# 队伍匹配状态：正在排队中
MATCH_STATUS_TEAM_MATCHED				= 3			# 队伍匹配状态：匹配成功
# 匹配成功状态
MATCH_STATUS_TEAM_MATCH_PLENARY			= 0			# 队伍匹配状态：匹配的满员队伍
MATCH_STATUS_TEAM_MATCH_VACANT			= 1			# 队伍匹配状态：匹配的残缺队伍
# 职责确认状态
MATCHED_CONFIRM_STATUS_ACCEPT			= 1			# 职责确认状态，接受
MATCHED_CONFIRM_STATUS_ABANDON			= 2			# 职责确认状态，放弃
MATCHED_CONFIRM_STATUS_PENDING			= 3			# 职责确认状态，未选择
# 玩家离队原因定义
LEAVE_TEAM_ACTIVE						= 1			# 玩家自己主动离队
LEAVE_TEAM_VOTE_KICKED					= 2			# 被投票踢出队伍
LEAVE_TEAM_ABANDON_MATCH				= 3			# 放弃半路副本从而离开队伍




# 野外副本 monster type 定义
MONSTER_TYPE_COMMON_MONSTER 	= 0 	# 小怪
MONSTER_TYPE_COMMON_BOSS		= 1		# boss
MONSTER_TYPE_NON_METERING		= 2 	# 不计算怪物

# 抗性
RESIST_NONE			 = 0
RESIST_YUANLI		 = 1
RESIST_LINGLI		 = 2
RESIST_TIPO			 = 3


#-----------------------------------------------------------------------------
# 盘古守护系统
#-----------------------------------------------------------------------------

# 盘古守护行为
PGNAGUAL_ACTION_MODE_FOLLOW 			= 0					# 跟随
PGNAGUAL_ACTION_MODE_ATTACK 			= 1					# 攻击
PGNAGUAL_ACTION_MODE_NEAR_GROUP 		= 2					# 近战群体类盘古守护使用技能
PGNAGUAL_ACTION_MODE_NEAR_SINGLE 		= 3					# 近战单体类盘古守护使用技能
PGNAGUAL_ACTION_MODE_FAR_PHYSIC 		= 4					# 远程物理类盘古守护使用技能
PGNAGUAL_ACTION_MODE_FAR_MAGIC 			= 5					# 远程法术类盘古守护使用技能

# 盘古守护攻击类型定义
PGNAGUAL_TYPE_NEAR_GROUP 				= 1					# 近战群体
PGNAGUAL_TYPE_NEAR_SINGLE 				= 2					# 近战单体
PGNAGUAL_TYPE_FAR_PHYSIC 				= 3					# 远程物理
PGNAGUAL_TYPE_FAR_MAGIC 				= 4					# 远程法术

# 盘古守护及造化玉牒的境界
PGNAGUAL_REALM_DIXIAN 					= 1 				# 地仙
PGNAGUAL_REALM_TIANXIAN 				= 2 				# 天仙
PGNAGUAL_REALM_TAIYISANXIAN 			= 3 				# 太乙散仙
PGNAGUAL_REALM_DALUOJINXIAN 			= 4 				# 大罗金仙
PGNAGUAL_REALM_ZHUNSHENG 				= 5 				# 准圣

# 阵形
PG_FORMATION_TYPE_CIRCLE		= 1 	# 方圆阵
PG_FORMATION_TYPE_SNAKE			= 2 	# 长蛇阵
PG_FORMATION_TYPE_FISH			= 3 	# 鱼鳞阵
PG_FORMATION_TYPE_ARROW			= 4 	# 箭矢阵
PG_FORMATION_TYPE_GOOSE			= 5 	# 雁行阵
PG_FORMATION_TYPE_CRANE			= 6 	# 鹤翼阵
PG_FORMATION_TYPE_MOON			= 7 	# 偃月阵
PG_FORMATION_TYPE_EIGHT			= 8 	# 八卦阵
PG_FORMATION_TYPE_TAIL			= 9 	# 尾行阵
PG_FORMATION_TYPE_SCATTERED		= 10 	# 分散阵

#-----------------------------------------------------------------------------
# 英雄联盟版失落宝藏
#-----------------------------------------------------------------------------
YXLM_MAX_EQUIP_AMOUNT			= 6		# 最多能有6件装备


#-----------------------------------------------------------------------------
# 帮会相关操作的各种原因
#-----------------------------------------------------------------------------
# 创建帮会原因
TONG_CREATE_REASON_NOMAL		= 1		# 正常流程创建
TONG_CREATE_REASON_GM			= 2		# GM指令创建

# 删除帮会原因

TONG_DELETE_REASON_GM				= 1		# GM指令删除
TONG_DELETE_REASON_MONEY_LESS		= 2		# 建筑维护费不够
TONG_DELETE_REASON_ACTIVITY_LOW		= 3		# 活跃度太低
TONG_DELETE_REASON_NOMAL			= 4		# 正常解散

# 帮主变更原因
TONG_CHIEF_CHANGE_REASON			= 1		# 帮主退位

# 帮会声望改变原因
TONG_PRESTIGE_CHANGE_REST			= 0		# 其他原因
TONG_PRESTIGE_CHANGE_GM				= 1		# GM指令
TONG_PRESTIGE_CHANGE_USE_ITEM		= 2		# 使用物品改变帮会声望
TONG_PRESTIGE_CHANGE_ABA			= 3		# 帮会擂台
TONG_PRESTIGE_CHANGE_ROB_WAR		= 4		# 帮会夺城

# 帮会等级改变原因
TONG_LEVEL_CHANGE_REASON_GM				= 0		# GM指令
TONG_LEVEL_CHANGE_REASON_TERRITORY		= 1		# 帮会建筑升级


#称号改变原因
ALLY_TITILE_CHANGE_REASON_INIT			= 0		# 初始化结拜称号
ALLY_TITILE_CHANGE_REASON_ADD			= 1		# 增加结拜称号
ALLY_TITILE_CHANGE_REASON_ADD_MEMBER		= 2		# 增加结拜称号
ALLY_TITILE_CHANGE_REASON_MEMBER_CHANGE			= 3		# 更改结拜称号





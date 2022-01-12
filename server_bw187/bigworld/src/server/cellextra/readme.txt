这个目录放置对cell的c++扩展
用gencsdefine.py产生csdefine.py对应的csdefine.h

注：已不再需要以下方式进行extra的初始化
-----------------------------------------------------------------
再entity初始化时要调用InitXXXXXExtra来初始化扩展类参数
注:GameObjectExtra类的扩展初始化函数InitGameObjectExtra
class GameObject( BigWorld.Entity, ECBExtend.ECBExtend ):
    """
    """
    def __init__( self ):
        """
        """
        BigWorld.Entity.__init__( self )
        ECBExtend.ECBExtend.__init__( self )

        script = self.getScript()
        if script:
            script.initEntity( self )
        self.InitGameObjectExtra()
-----------------------------------------------------------------

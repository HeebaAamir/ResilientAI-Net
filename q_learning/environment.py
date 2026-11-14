class NetworkEnvironment:
    def __init__(self):
        #definig environment
        # two paths of diamond network
        self.paths = {
            0: ["S", "A", "D"],  #upper path
            1: ["S", "B", "D"]   #lower path
        }

        #initial conditions
        self.path_conditions = {
            0: {
                "available": True,
                "delay": 10,
                "packet_loss": 0.0
            },

            1: {
                "available": True,
                "delay": 15,
                "packet_loss": 0.0
            }
        }
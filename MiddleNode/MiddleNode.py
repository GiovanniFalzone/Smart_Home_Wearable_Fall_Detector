import sys, signal, time, threading
from time import localtime, strftime
from WearableManager import WearableManager
from WebServerLink_Manager import WebServerLink_Manager
from Wearable_Simulator import Wearable_Simulator

def signal_handler(signal, frame):
	print('Running shutdown procedure')
	for i in workers :
		i.stop()
	now = strftime("%y-%m-%d %H:%M:%S", localtime())
	msg = {'User_id':0, 'User_name':'root', 'Dev_id':0, 'Dev_name':'MiddleNode', 'Value_type':'Status', 'Value':0, 'DateTime':now}
	msg = {'Type':'Json', 'Data':msg}
	SendBuffer.insert_msg(msg)
	for i in workers :
		i.join()
	time.sleep(5)
	sys.exit(0)

class SendBuffer:
	def __init__(self) :
		self.condition = threading.Condition()
		self.SendList = []

	def insert_msg(self, data):
		self.condition.acquire()
		self.SendList.append(data)
		self.condition.notify()
		self.condition.release()

	def empty(self) :
		self.condition.acquire()
		if(len(self.SendList)==0):
			self.condition.wait()
		tmp = self.SendList
		self.SendList = []
		self.condition.release()
		return tmp

	def howmany(self) :
		return len(self.SendList)

WebServer_url = "http://uhbi.altervista.org"

workers = []
SendBuffer = SendBuffer()

if __name__ == "__main__":
	signal.signal(signal.SIGINT, signal_handler)
	signal.signal(signal.SIGTERM, signal_handler)

	workers.append(WebServerLink_Manager(WebServer_url, SendBuffer, 1))
	workers.append(WearableManager(SendBuffer))
#	workers.append(Wearable_Simulator(SendBuffer, 3, 'Roberto', 5, 'Simulated Fall Detector', 0))
#	workers.append(Wearable_Simulator(SendBuffer, 4, 'Giovanni', 2, 'Simulated Fall Detector', 0))
#	workers.append(Wearable_Simulator(SendBuffer, 5, 'Alessandro', 3, 'Simulated Fall Detector', 0))

	while 1 :
		time.sleep(1)
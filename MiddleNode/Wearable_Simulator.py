from threading import *
import threading
from time import localtime, strftime
import socket
import random

class Wearable_Simulator(Thread):
	def __init__(self, SendBuffer, Period = 2, User_Name = 'Tester', User_Id = 0, Dev_Name = 'Tester', Dev_Id = 0):
		Thread.__init__(self)
		self._stop_event = Event()
		self._interval = Period
		self.iteration = 0
		self.SendBuffer = SendBuffer
		self.Dev_name = Dev_Name
		self.Dev_id = Dev_Id
		self.User_name = User_Name
		self.User_id = User_Id

		self.client = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
		self.client.connect(('127.0.0.1', 2048))
		print('Wearable ' + self.Dev_name + ' of User' + self.User_name + ' is working')
		msg = 'W>>'+ self.User_name +','+ str(self.User_id) +','+ self.Dev_name +','+ str(self.Dev_id) + '//'
		self.client.send(msg.encode())
		self.start()

	def stop(self):
		self._stop_event.set()

	def run(self):
		while not self._stop_event.isSet():
			self.task()
			self._stop_event.wait(self._interval)
		print(self.User_name + ' Task stopped')

	def task(self):
		msg = 'S>>-1,-2,-3,-4,-5,-6//'
		rand = random.randint(1,100)
		if (rand) <= 25:
			msg='A>>//'
		elif(rand) <= 50 :
			msg='F>>//'
		else:
			return
		try :
			self.client.send(msg.encode())
		except ConnectionAbortedError:
			self.stop()
from threading import *
import socket
import time
from time import localtime, strftime
import json

class Wearable_FallDetector(Thread):
	def __init__(self, socket, address, SendBuffer, BufferedSamples=6000):
		Thread.__init__(self)
		self.MAX_TIMED_WORKERS = 1
		self._stop_event = Event()
		self.SendBuffer = SendBuffer
		self.Dev_name = ''
		self.Dev_id = 0
		self.User_name = ''
		self.User_id = 0
		self.sock = socket
		self.sock.settimeout(32)
		self.addr = address

		self.BufferedSamples = BufferedSamples
		self.count = 0
		self.top = 0
		self.lines = []
		self.count_total	= 0
		self.count_duplicated = 0

		self.TimedSend_workers=None

		print("Connected with: ", str(self.addr))
		self.start()

	def stop(self):
		self._stop_event.set()

	def printBuffer(self) :
		for line in self.lines :
			print(line)

	def store(self, msg) :
		if self.count >= self.BufferedSamples :
			self.top = (self.top + 1)%self.BufferedSamples
			self.lines[self.top] = msg
		else :
			self.count = self.count + 1
			self.top = (self.top + 1)%self.BufferedSamples
			self.lines.append(msg);

	def record(self, Opt='-') :
		print('record\n');
		dir = 'signals/'
		filename = dir + Opt+'_'+strftime("%y-%m-%d_%H-%M-%S", localtime())
		filename = filename + '.csv'
		with open(filename, 'w') as writer:
			for line in self.lines :
				writer.write(line + '\n');
		return filename

	class TimedSend(Thread):
		def __init__(self, SendBuffer, msg, timeout=5) :
			Thread.__init__(self)
			self.SendIt = True
			self.Done = False
			self.timeout = timeout
			self.message = msg
			self.SendBuffer = SendBuffer
			self.start()

		def Cancel(self):
			self.SendIt = False
			print('Cancelled '+ strftime('%M:%S', localtime()))

		def run(self):
			print('Start timer '+ strftime('%M:%S', localtime()))
			time.sleep(self.timeout)
			if self.SendIt==True:
				self.SendBuffer.insert_msg(self.message)
				print('Sent '+ strftime('%M:%S', localtime()))
				self.Done = True

	def ComposeMessage(self, data_type, val):
		now = strftime("%y-%m-%d %H:%M:%S", localtime())
		msg = {'User_id':self.User_id, 'User_name':self.User_name, 'Dev_id':self.Dev_id, 'Dev_name':self.Dev_name, 'Value_type':data_type, 'Value':val, 'DateTime':now}
		msg = {'Type':'Json', 'Data':msg}
		return msg

	def SendFile(self, Opt='-'):
		filename = self.record(Opt)
		msg = {'Type':'File', 'Data':filename}
		self.SendBuffer.insert_msg(msg)

	def IFell(self):
		print(self.User_name + ' is fallen')
		msg = self.ComposeMessage('Falllen', 1)
		if (self.TimedSend_workers) :
			self.TimedSend_workers.Cancel()
			self.TimedSend_workers = None
		self.TimedSend_workers = self.TimedSend(self.SendBuffer, msg)
		self.SendFile('Fallen')

	def IamAlive(self):
		print(self.User_name + ' is alive')
		if (self.TimedSend_workers) :
			if (self.TimedSend_workers.Done==False) :
				self.TimedSend_workers.Cancel()
				self.TimedSend_workers = None
			else :
				msg = self.ComposeMessage('Alive', 1)
				self.SendBuffer.insert_msg(msg)
				self.TimedSend_workers = None

	def Wearable_commander(self, cmd, msg) :
		values = msg.split(',')
		if (cmd == 'S' and len(values)==6):
			self.count_total = self.count_total + 1
			self.store(msg)
		elif cmd == 'R' :
			self.SendFile('Record')
		elif cmd == 'A' :
			self.IamAlive()
		elif cmd == 'F' :
			self.IFell()
		elif cmd == 'W' and len(values)==4:
			self.User_name = values[0]
			self.User_id = values[1]
			self.Dev_name = values[2]
			self.Dev_id = values[3]
			print(self.User_name+'\'s '+self.Dev_name+' is connected')

		else :
#			print('Command not Recognized from ' + self.User_name + '\'s ' + self.Dev_name + ' CMD: '+ cmd + ' MSG: ' + msg);
			if cmd == 'S':
				self.count_duplicated = self.count_duplicated + 1
#				print("Rate Duplicated: " + str((self.count_duplicated/self.count_total)*100));
				self.store(self.lines[self.top-1])

	def run(self):
		while not self._stop_event.isSet():
			msg = ''
			try:
				msg = self.sock.recv(8192).decode('utf-8')
			except (ConnectionError) :
				print('Connection problem for '+self.User_name)
				break
			except socket.timeout:
				print('Task for '+self.User_name+' Timed out')
				break

			msg_lines = msg.split('//')
			for i in msg_lines :
				myline = i.strip(' \t\n\r/\\').split('>>')
				if len(myline) == 2 :
					self.Wearable_commander(myline[0], myline[1])

		if self.TimedSend_workers :
			self.TimedSend_workers.join()
		print('Wearable Task for ' + self.User_name + '\'s ' + self.Dev_name + ' Stopped');
		self.sock.close()
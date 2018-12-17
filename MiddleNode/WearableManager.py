from threading import *
import socket
from Wearable_FallDetector import Wearable_FallDetector

class WearableManager(Thread):
	def __init__(self, SendBuffer):
		Thread.__init__(self)
		self._stop_event = Event()
		self.SendBuffer = SendBuffer
		self.workers = []

		self.server_ip = ''		# aòll interfaces
		self.server_port = 2048
		self.sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
		self.sock.settimeout(1)
		print('Socket Created');
		try :
			self.sock.bind((self.server_ip, self.server_port));
		except socket.error as msg:
			print('Bind failed. Error Code : ' + str(msg[0]) + ' Message ' + msg[1])
			sys.exit()
		self.sock.listen(10)
		print("Socket bind completed, start listening")

		print("Wearable Listener is working")
		self.start()

	def stop(self):
		self._stop_event.set()

	def run(self):
		while not self._stop_event.isSet():
			try:
				conn, addr = self.sock.accept()		#wait to accept a connection - blocking call
				self.workers.append(Wearable_FallDetector(conn, addr, self.SendBuffer))
			except socket.timeout:
				pass

		self.sock.close()
		for i in self.workers :
			i.stop()
		for i in self.workers :
			i.join()
		print('WearableManager Task stopped')
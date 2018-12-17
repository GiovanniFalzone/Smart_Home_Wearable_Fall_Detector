from threading import *
import requests, json

class WebServerLink_Manager(Thread):
	def __init__(self, Server_url, SendBuffer, Period=5):
		Thread.__init__(self)
		self._stop_event = Event()
		self.Server_url = Server_url
		self.SendBuffer = SendBuffer
		self.iteration = 0
		self._interval = Period
		print("Periodic Task Web Comunication crated, period: "+ str(self._interval))
		self.start()

	def stop(self):
		self._stop_event.set()

	def run(self):
		while not self._stop_event.isSet():
			self.iteration = self.iteration + 1
			self.task()
			self._stop_event.wait(self._interval)
		print('WebServerLink_Manager Task stopped')

	def printResponse(self, resp):
#		print('-----------------Request----------------------------')
#		print(resp.request.headers)
#		print(resp.request.body)
		print('-----------------Response----------------------------')
		print(resp.status_code, resp.reason)
		print(resp.text)
		print('-----------------------------------------------------')

	def SendFile(self, filename, page_url) :
		print('Send File: '+ filename)
		fin = open(filename, 'rb')
		file = {'file': fin}
		try:
			response = requests.post(page_url, files=file)
			self.printResponse(response)
		except Exception as e:
			print(e)
			print('Connection to Server problem')
		finally:
			fin.close()

	def task(self):
		print("PeriodicTask Send to server");
		page_url = self.Server_url + "/php/postNotification.php"
		msg = self.SendBuffer.empty()
		ToSend = []
		for msg_i in msg :
			msg_i_type = msg_i['Type']
			if(msg_i_type=='File') :
				msg.remove(msg_i)
				self.SendFile(msg_i['Data'], page_url)

			elif(msg_i_type=='Json') :
				ToSend.append(msg_i['Data']) 

		if (len(ToSend) > 0) :
			try:
				Payload = ToSend
				response = requests.post(page_url, json=Payload)
				self.printResponse(response)
			except Exception as e:
				print(e)
				print('Connection to Server problem')
from time import localtime, strftime
import socket, random

Dev_name = 'Terminal'
Dev_id = 0
User_name = 'Gionni'
User_id = 1

client = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
client.connect(('127.0.0.1', 2048))
print('Wearable ' + Dev_name + ' of User' + User_name + ' is working')
msg = 'W>>'+ User_name +','+ str(User_id) +','+ Dev_name +','+ str(Dev_id) + '//'
client.send(msg.encode())

while 1:
	cmd = input('>> ')
	msg = []
	if cmd=='Alive':
		msg.append('A>>//')
	elif cmd=='Fallen':
		msg.append('F>>//')
	elif cmd == 'Record':
		msg.append('R>>//')
	elif cmd == 'Samples':
		for i in range(1, 6000) :
			Ax = (2**15)*random.uniform(-1,1)
			Ay = (2**15)*random.uniform(-1,1)
			Az = (2**15)*random.uniform(-1,1)
			Gx = (2**15)*random.uniform(-1,1)
			Gy = (2**15)*random.uniform(-1,1)
			Gz = (2**15)*random.uniform(-1,1)
			msg.append('S>>'+'%6d'%Ax+','+'%6d'%Ay+','+'%6d'%Az+','+'%6d'%Gx+','+'%6d'%Gy+','+'%6d'%Gz+'//')
	else:
		print('Command not recognized')
	try :
		for i in msg :
			client.send(i.encode())
	except ConnectionAbortedError:
		client.stop()
	finally:
		print(msg)
#include "I2Cdev.h"
#include "MPU6050.h"
#if I2CDEV_IMPLEMENTATION == I2CDEV_ARDUINO_WIRE
    #include "Wire.h"
#endif

// #define DEBUG_SERIAL
// #define DEBUG_RT_SERIAL

#ifdef DEBUG_RT_SERIAL
	#define DEBUG_RT_PRINT(x)  Serial.print (x)
	#define DEBUG_RT_PRINTLN(x)  Serial.println (x)
#else
	#define DEBUG_RT_PRINT(x)
	#define DEBUG_RT_PRINTLN(x)
#endif

#ifdef DEBUG_SERIAL
	#define DEBUG_PRINT(x)  Serial.print (x)
	#define DEBUG_PRINTLN(x)  Serial.println (x)
#else
	#define DEBUG_PRINT(x)
	#define DEBUG_PRINTLN(x)
#endif

#define SERIAL_BUFFER_SIZE 256

//#define MPU6050_GYRO_FS_250         0x00
//#define MPU6050_GYRO_FS_500         0x01
//#define MPU6050_GYRO_FS_1000        0x02
//#define MPU6050_GYRO_FS_2000        0x03
//
//#define MPU6050_ACCEL_FS_2          0x00
//#define MPU6050_ACCEL_FS_4          0x01
//#define MPU6050_ACCEL_FS_8          0x02
//#define MPU6050_ACCEL_FS_16         0x03

// offset 2g
#define AX_OFFSET       -3861
#define AY_OFFSET       -1551
#define AZ_OFFSET       1628
#define GX_OFFSET       43
#define GY_OFFSET       13
#define GZ_OFFSET       61

#define ACC_RES 				16
#define GYRO_RES 				2000

#define ACC_CONVERSION_RATE			((float)(((uint16_t)1)<<15))/((float)ACC_RES)
#define GYRO_CONVERSION_RATE		((float)(((uint16_t)1)<<15))/((float)GYRO_RES)

#define ACC_SQ_CONVERSION_RATE		pow(((float)(((uint16_t)1)<<15))/((float)ACC_RES), 2)
#define GYRO_SQ_CONVERSION_RATE		pow(((float)(((uint16_t)1)<<15))/((float)GYRO_RES), 2)

#define MPU_ACC_RES 			MPU6050_ACCEL_FS_16
#define MPU_GYRO_RES 			MPU6050_GYRO_FS_2000

#define MPU6050_REGISTER_ACCEL_XOUT_H		0x3B
#define MPU6050_SLAVE_ADDRESS				uint8_t(0x68)

#define MSG_LEN 				3 + (1+6+1)*5 + 6 + 2    // inviati //3 + (1+6+1)*5 + 6 + 2

#define FALL_LED_PIN			4	// per indicare una caduta
#define RECORD_PIN 				8   // per far salvare gli ultimi N campioni come file
#define FALL_RESET_PIN			9   //  per inviare un segnale di reset della caduta

//---------------------------------------------------------------------------------------------------------
#define FALL_ACC_THRESHOLD_IN_G					16
#define FALL_ACC_THRESHOLD 						(uint32_t)(pow(FALL_ACC_THRESHOLD_IN_G, 2)*ACC_SQ_CONVERSION_RATE)

#define FALL_GYRO_THRESHOLD_IN_DPS				1050 
#define FALL_GYRO_THRESHOLD 					(uint32_t)(pow(FALL_GYRO_THRESHOLD_IN_DPS, 2)*GYRO_SQ_CONVERSION_RATE)

//----------------------------------------------------------------------------------------------------------
#define DYING_ACC_LOW_THRESHOLD_IN_G			0.8	
#define DYING_ACC_LOW_THRESHOLD 				(uint32_t)(pow(DYING_ACC_LOW_THRESHOLD_IN_G, 2)*ACC_SQ_CONVERSION_RATE)

#define DYING_ACC_HIGH_THRESHOLD_IN_G			1.2
#define DYING_ACC_HIGH_THRESHOLD 				(uint32_t)(pow(DYING_ACC_HIGH_THRESHOLD_IN_G, 2)*ACC_SQ_CONVERSION_RATE)

#define DYING_GYRO_THRESHOLD_IN_DPS				230
#define DYING_GYRO_THRESHOLD 					(uint32_t)(pow(DYING_GYRO_THRESHOLD_IN_DPS, 2)*GYRO_SQ_CONVERSION_RATE)

//-----------------------------------------------------------------------------------------------------
#define DYING_POSITION_AX_THRESHOLD_IN_G		0.4
#define DYING_POSITION_AX_THRESHOLD				(uint32_t)(DYING_POSITION_AX_THRESHOLD_IN_G * ACC_CONVERSION_RATE)

#define DYING_POSITION_GX_THRESHOLD_IN_DPS		180
#define DYING_POSITION_GX_THRESHOLD 			(uint32_t)(DYING_POSITION_GX_THRESHOLD_IN_DPS * GYRO_CONVERSION_RATE)

#define DYING_POSITION_GY_THRESHOLD_IN_DPS		90
#define DYING_POSITION_GY_THRESHOLD 			(uint32_t)(DYING_POSITION_GY_THRESHOLD_IN_DPS * GYRO_CONVERSION_RATE)

#define DYING_POSITION_GZ_THRESHOLD_IN_DPS		90
#define DYING_POSITION_GZ_THRESHOLD				(uint32_t)(DYING_POSITION_GZ_THRESHOLD_IN_DPS * GYRO_CONVERSION_RATE)
//-------------------------------------------------------------------------------------------------------

#define SAMPLING_PERIOD_IN_MS			20							// 10ms -> 100Hz
#define SAMPLING_PERIOD					SAMPLING_PERIOD_IN_MS*1000	// in us

#define DYING_MIN_TIMER_IN_SEC			3
#define DYING_COUNTER_THRESHOLD			uint32_t(DYING_MIN_TIMER_IN_SEC*(1000/SAMPLING_PERIOD_IN_MS))	// sec in funzione della frequenza di campionamento

#define DYING_TIMEOUT_IN_SEC			6
#define DYING_COUNTER_TIMEOUT			uint32_t(DYING_TIMEOUT_IN_SEC*(1000/SAMPLING_PERIOD_IN_MS))

#define DIM_CIRCULAR_ARRAY				10


MPU6050 accelgyro;		// size 15 byte

struct motion{
	int16_t ax;
	int16_t ay;
	int16_t az;
	int16_t gx;
	int16_t gy;
	int16_t gz;
};

struct motionCircularArray{
	motion mval_Array[DIM_CIRCULAR_ARRAY];
	uint8_t head = 0;
	uint8_t tail = 0;
	uint8_t alert = 0;
	uint16_t count = 0;
	uint16_t count_dying = 0;
	uint16_t count_since_peak = 0;
} LastMotions;

/*Prima sposto gli indici e poi salvo così il valore puntato da head è l'ultimo salvato*/
void saveMotionInBuffer(motion mval){
	LastMotions.head = (LastMotions.head + 1)%DIM_CIRCULAR_ARRAY;
	if(LastMotions.count==DIM_CIRCULAR_ARRAY){
		LastMotions.tail = (LastMotions.tail + 1)%DIM_CIRCULAR_ARRAY;
	} else {
		LastMotions.count++;
	}
	LastMotions.mval_Array[LastMotions.head] = mval;
	DEBUG_PRINT("Head: "); DEBUG_PRINT(LastMotions.head);
	DEBUG_PRINT(" Tail: "); DEBUG_PRINT(LastMotions.tail);
	DEBUG_PRINT(" Count: "); DEBUG_PRINTLN(LastMotions.count);
}

motion Read_MotionValue() {
	motion mval;
	Wire.beginTransmission(MPU6050_SLAVE_ADDRESS);
	Wire.write(MPU6050_REGISTER_ACCEL_XOUT_H);
	Wire.endTransmission();
	Wire.requestFrom(MPU6050_SLAVE_ADDRESS, (uint8_t)14);
	accelgyro.getMotion6(&(mval.ax), &(mval.ay), &(mval.az), &(mval.gx), &(mval.gy), &(mval.gz));
	saveMotionInBuffer(mval);
	return mval;
}

void toggleLed(uint8_t pin, uint8_t n){
	uint8_t led_status = LOW;
	for(uint8_t i=0; i<n; i++){
		digitalWrite(pin, led_status);
		delay(500/n);
		led_status = !led_status;
	}
}

void IamAlive(){
	Serial.println("A>>//");
	toggleLed(FALL_LED_PIN, 7);
}

void RecordData(){
	Serial.println("R>>//");
	toggleLed(FALL_LED_PIN, 7);  
}

void checkFall(motion mval){
	uint32_t ax_2, ay_2, az_2, acc_sqare_sum;
	uint32_t gx_2, gy_2, gz_2, gyro_sqare_sum;
	bool checkDying_Acc, checkDying_Gyro, checkDying_Position;

	ax_2 = ((uint32_t)mval.ax*(uint32_t)mval.ax);
	ay_2 = ((uint32_t)mval.ay*(uint32_t)mval.ay);
	az_2 = ((uint32_t)mval.az*(uint32_t)mval.az);
	acc_sqare_sum = (ax_2 + ay_2 + az_2);

	gx_2 = ((uint32_t)mval.gx*(uint32_t)mval.gx);
	gy_2 = ((uint32_t)mval.gy*(uint32_t)mval.gy);
	gz_2 = ((uint32_t)mval.gz*(uint32_t)mval.gz);
	gyro_sqare_sum = (gx_2 + gy_2 + gz_2);

	if((acc_sqare_sum > FALL_ACC_THRESHOLD) && (gyro_sqare_sum > FALL_GYRO_THRESHOLD)){
		LastMotions.alert = 1;
		LastMotions.count_dying = 0;
		LastMotions.count_since_peak = 0;
	}

	if(LastMotions.alert){
		if (LastMotions.count_since_peak < DYING_COUNTER_TIMEOUT){
			LastMotions.count_since_peak++;

			checkDying_Acc = (acc_sqare_sum > DYING_ACC_LOW_THRESHOLD);
			checkDying_Acc = checkDying_Acc && (acc_sqare_sum < DYING_ACC_HIGH_THRESHOLD);

			checkDying_Gyro = (gyro_sqare_sum < DYING_GYRO_THRESHOLD);

			checkDying_Position = (abs(mval.ax) < DYING_POSITION_AX_THRESHOLD);
			checkDying_Position = checkDying_Position && (abs(mval.gx) < DYING_POSITION_GX_THRESHOLD);
			checkDying_Position = checkDying_Position && (abs(mval.gy) < DYING_POSITION_GY_THRESHOLD);
			checkDying_Position = checkDying_Position && (abs(mval.gz) < DYING_POSITION_GZ_THRESHOLD);

			if(checkDying_Acc && checkDying_Position){
				LastMotions.count_dying++;
			}

			if(LastMotions.count_dying >= DYING_COUNTER_THRESHOLD) {
				LastMotions.alert = 0;
				LastMotions.count_dying = 0;
				LastMotions.count_since_peak = 0;
				Serial.println("F>>//");
				toggleLed(FALL_LED_PIN, 7);
			}
		} else {
			LastMotions.alert = 0;
			LastMotions.count_dying = 0;
			LastMotions.count_since_peak = 0;
		}
	}
	DEBUG_PRINT("Square: "); DEBUG_PRINT(ax_2); DEBUG_PRINT(" ");	DEBUG_PRINT(ay_2); DEBUG_PRINT(" ");	DEBUG_PRINT(az_2);
	DEBUG_PRINT(" ");						DEBUG_PRINT(gx_2); DEBUG_PRINT(" ");	DEBUG_PRINT(gy_2); DEBUG_PRINT(" ");	DEBUG_PRINTLN(gz_2);
	DEBUG_PRINT("Square Sum, Acc: "); DEBUG_PRINT(acc_sqare_sum); DEBUG_PRINT(", Gyro: "); DEBUG_PRINTLN(gyro_sqare_sum);
	DEBUG_PRINT("Fall Threshold, Acc: "); DEBUG_PRINT(FALL_ACC_THRESHOLD); DEBUG_PRINT(", Gyro: "); DEBUG_PRINTLN(FALL_GYRO_THRESHOLD);
	DEBUG_PRINT("Dying Threshold, Acc: "); DEBUG_PRINT(DYING_ACC_LOW_THRESHOLD); DEBUG_PRINT(" "); DEBUG_PRINT(DYING_ACC_HIGH_THRESHOLD);
	DEBUG_PRINT(", Gyro: "); DEBUG_PRINTLN(DYING_GYRO_THRESHOLD);
	DEBUG_PRINT("Alert: "); DEBUG_PRINT(LastMotions.alert);	DEBUG_PRINT(" Count Dying: "); DEBUG_PRINT(LastMotions.count_dying);
	DEBUG_PRINT(" Count since Peak: "); DEBUG_PRINTLN(LastMotions.count_since_peak);
}

void InitHandshake(){
	while(Serial.available()) Serial.read();
	uint8_t count = 0;
	while((count < 4) || (Serial.available()==0)){
		Serial.write('G');
		if(Serial.available()){
			if(Serial.read()=='G') count++;
			else {
				count = 0;
				while(Serial.available()) Serial.read();
			}
		}
		toggleLed(FALL_LED_PIN, 3);
	}
	for(count=0;count<4;count++)
		Serial.write('G');
	Serial.flush();
	while(Serial.available()) Serial.read();
	delay(1000);
	Serial.println("W>>Gionni,1,FallDetector,1");
}

void setup() {
	// join I2C bus (I2Cdev library doesn't do this automatically)
	#if I2CDEV_IMPLEMENTATION == I2CDEV_ARDUINO_WIRE
		Wire.begin();
	#elif I2CDEV_IMPLEMENTATION == I2CDEV_BUILTIN_FASTWIRE
		Fastwire::setup(400, true);
	#endif
	pinMode(FALL_LED_PIN, OUTPUT);
	digitalWrite(FALL_LED_PIN, LOW);    
	toggleLed(FALL_LED_PIN, 7);

	pinMode(FALL_RESET_PIN, INPUT);
	pinMode(RECORD_PIN, INPUT);

	Wire.setClock(400000L);
	Serial.begin(115200);

	DEBUG_PRINTLN("Initializing I2C devices...");
	accelgyro.initialize();
	accelgyro.setFullScaleGyroRange(MPU_GYRO_RES);
	accelgyro.setFullScaleAccelRange(MPU_ACC_RES);
	DEBUG_PRINTLN("Testing device connections...");
	DEBUG_PRINTLN(accelgyro.testConnection() ? "MPU6050 connection successful" : "MPU6050 connection failed");

	accelgyro.setXGyroOffset(GX_OFFSET);
	accelgyro.setYGyroOffset(GY_OFFSET);
	accelgyro.setZGyroOffset(GZ_OFFSET);
	accelgyro.setXAccelOffset(AX_OFFSET);
	accelgyro.setYAccelOffset(AY_OFFSET);
	accelgyro.setZAccelOffset(AZ_OFFSET);

	toggleLed(FALL_LED_PIN, 7);


	InitHandshake();

	DEBUG_PRINT("Conversion Rate Acc: "); DEBUG_PRINT(ACC_CONVERSION_RATE);DEBUG_PRINT(" Conversion Rate Gyro: "); DEBUG_PRINTLN(GYRO_CONVERSION_RATE);
	DEBUG_PRINT("Square Conversion Rate Acc: "); DEBUG_PRINT(ACC_SQ_CONVERSION_RATE);DEBUG_PRINT(" Conversion Rate Gyro: "); DEBUG_PRINTLN(GYRO_SQ_CONVERSION_RATE);
	DEBUG_PRINT("Fall Threshold, Acc: "); DEBUG_PRINT(FALL_ACC_THRESHOLD); DEBUG_PRINT(", Gyro: "); DEBUG_PRINTLN(FALL_GYRO_THRESHOLD);
	DEBUG_PRINT("Dying Threshold, Acc: "); DEBUG_PRINT(DYING_ACC_LOW_THRESHOLD); DEBUG_PRINT(" "); DEBUG_PRINT(DYING_ACC_HIGH_THRESHOLD);
	DEBUG_PRINT(", Gyro: "); DEBUG_PRINTLN(DYING_GYRO_THRESHOLD);
	DEBUG_PRINT("Position Threshold, Ax: "); DEBUG_PRINT(DYING_POSITION_AX_THRESHOLD);
	DEBUG_PRINT(" Gy: "); DEBUG_PRINT(DYING_POSITION_GY_THRESHOLD);DEBUG_PRINT(" Gz: "); DEBUG_PRINTLN(DYING_POSITION_GZ_THRESHOLD);
}

void loop() {
	static uint8_t fall_pin_now = 0, record_pin_now = 0, fall_pin_old = 0, record_pin_old = 0, count= 0;
	static uint32_t elapsed = 0, oldmicros = 0;
	uint32_t start_job = 0;
	char str_buf[MSG_LEN] = {NULL};
	motion mval = {0, 0, 0, 0, 0, 0};

	start_job = micros();
	if((start_job - oldmicros) >= SAMPLING_PERIOD) {
		elapsed = (start_job - oldmicros);
		mval = Read_MotionValue();
		sprintf(str_buf, "S>>%6d,%6d,%6d,%6d,%6d,%6d//", mval.ax, mval.ay, mval.az, mval.gx, mval.gy, mval.gz);  //3 + (1+6+1)*5 + 6 + 2
		Serial.println(str_buf);
		checkFall(mval);
		DEBUG_RT_PRINT("Execution Time (in us)>>"); DEBUG_RT_PRINT(micros() - start_job); DEBUG_RT_PRINTLN("//");
		DEBUG_RT_PRINT("Start job difference(in us)>>"); DEBUG_RT_PRINT(elapsed); DEBUG_RT_PRINTLN("//");
		oldmicros = start_job;
	}

	if(Serial.available()){
		if(Serial.read()=='G') count++;
		if(count == 4){
			InitHandshake();
			count = 0;
		}
	}

	fall_pin_now = digitalRead(FALL_RESET_PIN);
	record_pin_now = digitalRead(RECORD_PIN);
	if((fall_pin_old == 0) && (fall_pin_now == 1))
		IamAlive();

	if((record_pin_old == 0) && (record_pin_now == 1))
		RecordData();
	fall_pin_old = fall_pin_now;
	record_pin_old = record_pin_now;
}


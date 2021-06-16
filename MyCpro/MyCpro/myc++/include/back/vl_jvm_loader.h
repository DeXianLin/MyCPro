
#ifndef JVM_WRAPPER_H_
#define JVM_WRAPPER_H_


//#include <jni.h>


//extern JavaVM *android_jvm;

#ifdef __cplusplus
#define ATTACH_JVM(jni_env)  \
	JNIEnv *g_env;\
	int env_status = android_jvm->GetEnv((void **)&g_env, JNI_VERSION_1_6); \
	jint attachResult = android_jvm->AttachCurrentThread(&jni_env,NULL);

#define DETACH_JVM(jni_env)   if( env_status == JNI_EDETACHED ){ android_jvm->DetachCurrentThread(); }
#else
#define ATTACH_JVM(jni_env)  \
	JNIEnv *g_env;\
	int env_status = (*android_jvm)->GetEnv(android_jvm, (void **)&g_env, JNI_VERSION_1_6); \
	jint attachResult = (*android_jvm)->AttachCurrentThread(android_jvm, &jni_env,NULL);

#define DETACH_JVM(jni_env)   if( env_status == JNI_EDETACHED ){ (*android_jvm)->DetachCurrentThread(android_jvm); }
#endif

#endif /* JVM_WRAPPER_H_ */



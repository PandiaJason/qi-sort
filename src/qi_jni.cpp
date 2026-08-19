#include <jni.h>
#include "../include/qi_radix.hpp"

extern "C" {

JNIEXPORT void JNICALL Java_com_qisort_QiSort_sortNative(
    JNIEnv* env,
    jclass clazz,
    jintArray data,
    jint length
) {
    if (!data || length <= 1) return;

    jint* body = env->GetIntArrayElements(data, NULL);
    if (!body) return;

    qi::sort(reinterpret_cast<uint32_t*>(body), static_cast<size_t>(length));

    env->ReleaseIntArrayElements(data, body, 0);
}

}

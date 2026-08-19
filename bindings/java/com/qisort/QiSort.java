package com.qisort;

import java.io.File;

/**
 * QI-Sort Java Native Binding (JNI / FFI Wrapper)
 * High-Performance Quantum-Inspired Adaptive Radix Sorting for Java int[] arrays.
 */
public class QiSort {

    static {
        loadNativeLibrary();
    }

    private static void loadNativeLibrary() {
        String libName = System.mapLibraryName("qisort");
        File projectRoot = new File(System.getProperty("user.dir"));

        File[] candidatePaths = new File[] {
            new File(projectRoot, libName),
            new File(projectRoot, "libqisort.dylib"),
            new File(projectRoot, "libqisort.so"),
            new File("/usr/local/lib/" + libName)
        };

        for (File path : candidatePaths) {
            if (path.exists()) {
                System.load(path.getAbsolutePath());
                return;
            }
        }

        try {
            System.loadLibrary("qisort");
        } catch (UnsatisfiedLinkError e) {
            throw new RuntimeException("Could not load native QI-Sort library (" + libName + "). Build libqisort first.", e);
        }
    }

    /**
     * Native JNI method declaration for sorting int[] array.
     */
    public static native void sortNative(int[] data, int length);

    /**
     * Sort a Java int[] array in-place using QI-Sort.
     * Note: Reinterprets bit pattern as 32-bit unsigned integers.
     * 
     * @param data Array to sort
     */
    public static void sort(int[] data) {
        if (data == null || data.length <= 1) return;
        sortNative(data, data.length);
    }
}

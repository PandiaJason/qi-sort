#define PY_SSIZE_T_CLEAN
#include <Python.h>
#include "../include/qi_apex.hpp"
#include "../include/qi_radix.hpp"

// ── sort_ptr(ptr_int, n) ────────────────────────────────────────────────────
// Primary fast path: accepts a raw C pointer as an unsigned 64-bit integer.
// Python side: qi_sort_cpp.sort_ptr(array.ctypes.data, len(array))
// This bypasses all buffer protocol issues across numpy/Python versions.
static PyObject* py_qi_sort_ptr(PyObject* self, PyObject* args) {
    unsigned long long ptr_int = 0;
    unsigned long long n = 0;
    if (!PyArg_ParseTuple(args, "KK", &ptr_int, &n)) return NULL;
    if (n > 1) {
        uint32_t* ptr = reinterpret_cast<uint32_t*>(static_cast<uintptr_t>(ptr_int));
        Py_BEGIN_ALLOW_THREADS
        qi::apex::sort(ptr, static_cast<size_t>(n));
        Py_END_ALLOW_THREADS
    }
    Py_RETURN_NONE;
}

static PyObject* py_qi_radix8_ptr(PyObject* self, PyObject* args) {
    unsigned long long ptr_int = 0;
    unsigned long long n = 0;
    if (!PyArg_ParseTuple(args, "KK", &ptr_int, &n)) return NULL;
    if (n > 1) {
        uint32_t* ptr = reinterpret_cast<uint32_t*>(static_cast<uintptr_t>(ptr_int));
        Py_BEGIN_ALLOW_THREADS
        qi::detail::radixSort8(ptr, static_cast<size_t>(n));
        Py_END_ALLOW_THREADS
    }
    Py_RETURN_NONE;
}

static PyObject* py_qi_radix11_ptr(PyObject* self, PyObject* args) {
    unsigned long long ptr_int = 0;
    unsigned long long n = 0;
    if (!PyArg_ParseTuple(args, "KK", &ptr_int, &n)) return NULL;
    if (n > 1) {
        uint32_t* ptr = reinterpret_cast<uint32_t*>(static_cast<uintptr_t>(ptr_int));
        Py_BEGIN_ALLOW_THREADS
        qi::detail::radixSort11(ptr, static_cast<size_t>(n));
        Py_END_ALLOW_THREADS
    }
    Py_RETURN_NONE;
}

static PyObject* py_qi_radix16_ptr(PyObject* self, PyObject* args) {
    unsigned long long ptr_int = 0;
    unsigned long long n = 0;
    if (!PyArg_ParseTuple(args, "KK", &ptr_int, &n)) return NULL;
    if (n > 1) {
        uint32_t* ptr = reinterpret_cast<uint32_t*>(static_cast<uintptr_t>(ptr_int));
        Py_BEGIN_ALLOW_THREADS
        qi::detail::radixSort16(ptr, static_cast<size_t>(n));
        Py_END_ALLOW_THREADS
    }
    Py_RETURN_NONE;
}

// ── sort(list) ──────────────────────────────────────────────────────────────
// Fallback for Python lists (converts to vector, sorts, writes back).
static PyObject* py_qi_sort(PyObject* self, PyObject* args) {
    PyObject* obj;
    if (!PyArg_ParseTuple(args, "O", &obj)) return NULL;

    if (PyList_Check(obj)) {
        Py_ssize_t size = PyList_Size(obj);
        if (size <= 1) Py_RETURN_NONE;
        std::vector<uint32_t> vec(size);
        for (Py_ssize_t i = 0; i < size; ++i)
            vec[i] = static_cast<uint32_t>(PyLong_AsUnsignedLong(PyList_GET_ITEM(obj, i)));
        if (PyErr_Occurred()) return NULL;
        Py_BEGIN_ALLOW_THREADS
        qi::sort(vec);
        Py_END_ALLOW_THREADS
        for (Py_ssize_t i = 0; i < size; ++i)
            PyList_SET_ITEM(obj, i, PyLong_FromUnsignedLong(vec[i]));
        Py_RETURN_NONE;
    }

    PyErr_SetString(PyExc_TypeError,
        "sort() takes a Python list. For NumPy arrays use sort_ptr(array.ctypes.data, len(array)).");
    return NULL;
}

static PyMethodDef QiSortMethods[] = {
    {"sort_ptr",    py_qi_sort_ptr,    METH_VARARGS, "qi::sort(ptr_int, n) — raw pointer interface"},
    {"radix8_ptr",  py_qi_radix8_ptr,  METH_VARARGS, "radixSort8(ptr_int, n)"},
    {"radix11_ptr", py_qi_radix11_ptr, METH_VARARGS, "radixSort11(ptr_int, n)"},
    {"radix16_ptr", py_qi_radix16_ptr, METH_VARARGS, "radixSort16(ptr_int, n)"},
    {"sort",        py_qi_sort,        METH_VARARGS, "qi::sort for Python lists"},
    {NULL, NULL, 0, NULL}
};

static struct PyModuleDef qisortmodule = {
    PyModuleDef_HEAD_INIT,
    "qi_sort_cpp",
    "Quantum-Inspired Adaptive Radix Sort — Native Engine",
    -1,
    QiSortMethods
};

PyMODINIT_FUNC PyInit_qi_sort_cpp(void) {
    return PyModule_Create(&qisortmodule);
}

#define PY_SSIZE_T_CLEAN
#include <Python.h>
#include "../include/qi_radix.hpp"

static PyObject* py_qi_sort(PyObject* self, PyObject* args) {
    PyObject* obj;
    if (!PyArg_ParseTuple(args, "O", &obj)) return NULL;

    Py_buffer view;
    if (PyObject_GetBuffer(obj, &view, PyBUF_WRITABLE | PyBUF_ND | PyBUF_STRIDES) < 0) {
        PyErr_Clear();
        if (PyList_Check(obj)) {
            Py_ssize_t n = PyList_Size(obj);
            if (n <= 1) Py_RETURN_NONE;
            std::vector<uint32_t> vec(n);
            for (Py_ssize_t i = 0; i < n; ++i) {
                vec[i] = static_cast<uint32_t>(PyLong_AsUnsignedLong(PyList_GET_ITEM(obj, i)));
            }
            Py_BEGIN_ALLOW_THREADS
            qi::sort(vec);
            Py_END_ALLOW_THREADS
            for (Py_ssize_t i = 0; i < n; ++i) {
                PyList_SET_ITEM(obj, i, PyLong_FromUnsignedLong(vec[i]));
            }
            Py_RETURN_NONE;
        }
        PyErr_SetString(PyExc_TypeError, "Expected a writable 32-bit uint32 buffer or list.");
        return NULL;
    }

    if (view.itemsize != sizeof(uint32_t) || !PyBuffer_IsContiguous(&view, 'C')) {
        PyBuffer_Release(&view);
        PyErr_SetString(PyExc_TypeError, "Buffer must be a 32-bit C-contiguous array (uint32).");
        return NULL;
    }

    uint32_t* ptr = reinterpret_cast<uint32_t*>(view.buf);
    size_t n = view.len / sizeof(uint32_t);

    if (n > 1) {
        Py_BEGIN_ALLOW_THREADS
        qi::sort(ptr, n);
        Py_END_ALLOW_THREADS
    }

    PyBuffer_Release(&view);
    Py_RETURN_NONE;
}

static PyObject* py_qi_radix8(PyObject* self, PyObject* args) {
    PyObject* obj;
    if (!PyArg_ParseTuple(args, "O", &obj)) return NULL;
    Py_buffer view;
    if (PyObject_GetBuffer(obj, &view, PyBUF_WRITABLE | PyBUF_ND | PyBUF_STRIDES) < 0) return NULL;
    uint32_t* ptr = reinterpret_cast<uint32_t*>(view.buf);
    size_t n = view.len / sizeof(uint32_t);
    if (n > 1) {
        Py_BEGIN_ALLOW_THREADS
        qi::detail::radixSort8(ptr, n, true);
        Py_END_ALLOW_THREADS
    }
    PyBuffer_Release(&view);
    Py_RETURN_NONE;
}

static PyObject* py_qi_radix11(PyObject* self, PyObject* args) {
    PyObject* obj;
    if (!PyArg_ParseTuple(args, "O", &obj)) return NULL;
    Py_buffer view;
    if (PyObject_GetBuffer(obj, &view, PyBUF_WRITABLE | PyBUF_ND | PyBUF_STRIDES) < 0) return NULL;
    uint32_t* ptr = reinterpret_cast<uint32_t*>(view.buf);
    size_t n = view.len / sizeof(uint32_t);
    if (n > 1) {
        Py_BEGIN_ALLOW_THREADS
        qi::detail::radixSort11(ptr, n, true);
        Py_END_ALLOW_THREADS
    }
    PyBuffer_Release(&view);
    Py_RETURN_NONE;
}

static PyObject* py_qi_radix16(PyObject* self, PyObject* args) {
    PyObject* obj;
    if (!PyArg_ParseTuple(args, "O", &obj)) return NULL;
    Py_buffer view;
    if (PyObject_GetBuffer(obj, &view, PyBUF_WRITABLE | PyBUF_ND | PyBUF_STRIDES) < 0) return NULL;
    uint32_t* ptr = reinterpret_cast<uint32_t*>(view.buf);
    size_t n = view.len / sizeof(uint32_t);
    if (n > 1) {
        Py_BEGIN_ALLOW_THREADS
        qi::detail::radixSort16(ptr, n, true);
        Py_END_ALLOW_THREADS
    }
    PyBuffer_Release(&view);
    Py_RETURN_NONE;
}

static PyMethodDef QiSortMethods[] = {
    {"sort", py_qi_sort, METH_VARARGS, "Adaptive qi::sort"},
    {"radix8", py_qi_radix8, METH_VARARGS, "Plain Radix-8 (4-pass)"},
    {"radix11", py_qi_radix11, METH_VARARGS, "Plain Radix-11 (3-pass)"},
    {"radix16", py_qi_radix16, METH_VARARGS, "Plain Radix-16 (2-pass)"},
    {NULL, NULL, 0, NULL}
};

static struct PyModuleDef qisortmodule = {
    PyModuleDef_HEAD_INIT,
    "qi_sort_cpp",
    "Quantum-Inspired Adaptive Radix Sort Native Engine",
    -1,
    QiSortMethods
};

PyMODINIT_FUNC PyInit_qi_sort_cpp(void) {
    return PyModule_Create(&qisortmodule);
}

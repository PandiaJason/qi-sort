#define PY_SSIZE_T_CLEAN
#include <Python.h>
#include "../include/qi_radix.hpp"

static inline bool get_u32_buffer(PyObject* obj, Py_buffer* view, uint32_t** ptr, size_t* n) {
    if (PyObject_GetBuffer(obj, view, PyBUF_WRITABLE) < 0) {
        PyErr_Clear();
        return false;
    }
    if (view->itemsize != sizeof(uint32_t) || !PyBuffer_IsContiguous(view, 'C')) {
        PyBuffer_Release(view);
        return false;
    }
    *ptr = reinterpret_cast<uint32_t*>(view->buf);
    *n = view->len / sizeof(uint32_t);
    return true;
}

static PyObject* py_qi_sort(PyObject* self, PyObject* args) {
    PyObject* obj;
    if (!PyArg_ParseTuple(args, "O", &obj)) return NULL;

    Py_buffer view;
    uint32_t* ptr = nullptr;
    size_t n = 0;

    if (get_u32_buffer(obj, &view, &ptr, &n)) {
        if (n > 1) {
            Py_BEGIN_ALLOW_THREADS
            qi::sort(ptr, n);
            Py_END_ALLOW_THREADS
        }
        PyBuffer_Release(&view);
        Py_RETURN_NONE;
    }

    if (PyList_Check(obj)) {
        Py_ssize_t size = PyList_Size(obj);
        if (size <= 1) Py_RETURN_NONE;
        std::vector<uint32_t> vec(size);
        for (Py_ssize_t i = 0; i < size; ++i) {
            vec[i] = static_cast<uint32_t>(PyLong_AsUnsignedLong(PyList_GET_ITEM(obj, i)));
        }
        Py_BEGIN_ALLOW_THREADS
        qi::sort(vec);
        Py_END_ALLOW_THREADS
        for (Py_ssize_t i = 0; i < size; ++i) {
            PyList_SET_ITEM(obj, i, PyLong_FromUnsignedLong(vec[i]));
        }
        Py_RETURN_NONE;
    }

    PyErr_SetString(PyExc_TypeError, "Expected a writable C-contiguous 32-bit uint32 buffer or Python list.");
    return NULL;
}

static PyObject* py_qi_radix8(PyObject* self, PyObject* args) {
    PyObject* obj;
    if (!PyArg_ParseTuple(args, "O", &obj)) return NULL;
    Py_buffer view;
    uint32_t* ptr = nullptr;
    size_t n = 0;
    if (!get_u32_buffer(obj, &view, &ptr, &n)) {
        PyErr_SetString(PyExc_TypeError, "Expected a writable C-contiguous 32-bit uint32 buffer.");
        return NULL;
    }
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
    uint32_t* ptr = nullptr;
    size_t n = 0;
    if (!get_u32_buffer(obj, &view, &ptr, &n)) {
        PyErr_SetString(PyExc_TypeError, "Expected a writable C-contiguous 32-bit uint32 buffer.");
        return NULL;
    }
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
    uint32_t* ptr = nullptr;
    size_t n = 0;
    if (!get_u32_buffer(obj, &view, &ptr, &n)) {
        PyErr_SetString(PyExc_TypeError, "Expected a writable C-contiguous 32-bit uint32 buffer.");
        return NULL;
    }
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

#define PY_SSIZE_T_CLEAN
#include <Python.h>
#include "../include/qi_radix.hpp"

// Native PyExtension function for fast zero-copy buffer sorting
static PyObject* py_qi_sort(PyObject* self, PyObject* args) {
    PyObject* obj;
    if (!PyArg_ParseTuple(args, "O", &obj)) {
        return NULL;
    }

    Py_buffer view;
    if (PyObject_GetBuffer(obj, &view, PyBUF_WRITABLE | PyBUF_ND | PyBUF_STRIDES) < 0) {
        PyErr_Clear();
        // Fallback for Python lists
        if (PyList_Check(obj)) {
            Py_ssize_t n = PyList_Size(obj);
            if (n <= 1) Py_RETURN_NONE;
            std::vector<uint32_t> vec(n);
            for (Py_ssize_t i = 0; i < n; ++i) {
                vec[i] = static_cast<uint32_t>(PyLong_AsUnsignedLong(PyList_GET_ITEM(obj, i)));
            }
            qi::sort(vec);
            for (Py_ssize_t i = 0; i < n; ++i) {
                PyList_SET_ITEM(obj, i, PyLong_FromUnsignedLong(vec[i]));
            }
            Py_RETURN_NONE;
        }
        PyErr_SetString(PyExc_TypeError, "Expected a writable buffer (e.g. NumPy uint32 array) or a list of integers.");
        return NULL;
    }

    if (view.itemsize != sizeof(uint32_t)) {
        PyBuffer_Release(&view);
        PyErr_SetString(PyExc_TypeError, "qi_sort buffer must be 32-bit (uint32, int32, or float32). Use data.astype(np.uint32).");
        return NULL;
    }

    if (!PyBuffer_IsContiguous(&view, 'C')) {
        PyBuffer_Release(&view);
        PyErr_SetString(PyExc_ValueError, "Buffer must be C-contiguous. Use np.ascontiguousarray(data).");
        return NULL;
    }

    uint32_t* ptr = reinterpret_cast<uint32_t*>(view.buf);
    size_t n = view.len / sizeof(uint32_t);

    if (n > 1) {
        qi::sort(ptr, n);
    }

    PyBuffer_Release(&view);
    Py_RETURN_NONE;
}

static PyMethodDef QiSortMethods[] = {
    {"sort", py_qi_sort, METH_VARARGS, "Sort a NumPy array or list in-place using qi::sort"},
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

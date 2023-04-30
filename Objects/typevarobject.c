// TypeVar, TypeVarTuple, and ParamSpec
#include "Python.h"
#include "pycore_object.h"  // _PyObject_GC_TRACK/UNTRACK
#include "pycore_typevarobject.h"
#include "pycore_unionobject.h"   // _Py_union_type_or
#include "structmember.h"

/*[clinic input]
class typevar "typevarobject *" "&_PyTypeVar_Type"
class paramspec "paramspecobject *" "&_PyParamSpec_Type"
class paramspecargs "paramspecattrobject *" "&_PyParamSpecArgs_Type"
class paramspeckwargs "paramspecattrobject *" "&_PyParamSpecKwargs_Type"
class typevartuple "typevartupleobject *" "&_PyTypeVarTuple_Type"
class typealias "typealiasobject *" "&_PyTypeAlias_Type"
class Generic "PyObject *" "&PyGeneric_Type"
[clinic start generated code]*/
/*[clinic end generated code: output=da39a3ee5e6b4b0d input=aa86741931a0f55c]*/

typedef struct {
    PyObject_HEAD
    const char *name;
    PyObject *bound;
    PyObject *evaluate_bound;
    PyObject *constraints;
    PyObject *evaluate_constraints;
    PyObject *default_;
    PyObject *evaluate_default;
    bool covariant;
    bool contravariant;
    bool infer_variance;
} typevarobject;

typedef struct {
    PyObject_HEAD
    const char *name;
    PyObject *default_;
    PyObject *evaluate_default;
} typevartupleobject;

typedef struct {
    PyObject_HEAD
    const char *name;
    PyObject *bound;
    PyObject *default_;
    PyObject *evaluate_default;
    bool covariant;
    bool contravariant;
    bool infer_variance;
} paramspecobject;

typedef struct {
    PyObject_HEAD
    const char *name;
    PyObject *type_params;
    PyObject *compute_value;
    PyObject *value;
} typealiasobject;

#include "clinic/typevarobject.c.h"

static PyObject *
call_typing_func_object(const char *name, PyObject *args)
{
    PyObject *typing = PyImport_ImportModule("typing");
    if (typing == NULL) {
        return NULL;
    }
    PyObject *func = PyObject_GetAttrString(typing, name);
    if (func == NULL) {
        Py_DECREF(typing);
        return NULL;
    }
    PyObject *result = PyObject_CallObject(func, args);
    Py_DECREF(func);
    Py_DECREF(typing);
    return result;
}

static PyObject *
type_check(PyObject *arg)
{
    // Calling typing.py here leads to bootstrapping problems
    if (Py_IsNone(arg)) {
        return Py_NewRef(Py_TYPE(arg));
    }
    // TODO: real error message
    PyObject *args = PyTuple_Pack(2, arg, Py_None);
    if (args == NULL) {
        return NULL;
    }
    PyObject *result = call_typing_func_object("_type_check", args);
    Py_DECREF(args);
    return result;
}

/*
 * Return a typing.Union. This is used as the nb_or (|) operator for
 * TypeVar and ParamSpec. We use this rather than _Py_union_type_or
 * (which would produce a types.Union) because historically TypeVar
 * supported unions with forward references, and we want to preserve
 * that behavior. _Py_union_type_or only allows a small set of
 * types.
 */
static PyObject *
make_union(PyObject *self, PyObject *other)
{
    PyObject *args = PyTuple_Pack(2, self, other);
    if (args == NULL) {
        return NULL;
    }
    PyObject *result = call_typing_func_object("_make_union", args);
    Py_DECREF(args);
    return result;
}

static PyObject *
caller(void)
{
    _PyInterpreterFrame *f = _PyThreadState_GET()->cframe->current_frame;
    if (f == NULL) {
        Py_RETURN_NONE;
    }
    if (f == NULL || f->f_funcobj == NULL) {
        Py_RETURN_NONE;
    }
    PyObject *r = PyFunction_GetModule(f->f_funcobj);
    if (!r) {
        PyErr_Clear();
        r = Py_None;
    }
    return Py_NewRef(r);
}

static PyObject *
typevartuple_unpack(PyObject *tvt)
{
    PyObject *typing = PyImport_ImportModule("typing");
    if (typing == NULL) {
        return NULL;
    }
    PyObject *unpack = PyObject_GetAttrString(typing, "Unpack");
    if (unpack == NULL) {
        Py_DECREF(typing);
        return NULL;
    }
    PyObject *unpacked = PyObject_GetItem(unpack, tvt);
    Py_DECREF(typing);
    Py_DECREF(unpack);
    return unpacked;
}

static int
contains_typevartuple(PyTupleObject *params)
{
    Py_ssize_t n = PyTuple_GET_SIZE(params);
    PyTypeObject *tp = PyInterpreterState_Get()->cached_objects.typevartuple_type;
    for (Py_ssize_t i = 0; i < n; i++) {
        PyObject *param = PyTuple_GET_ITEM(params, i);
        if (Py_IS_TYPE(param, tp)) {
            return 1;
        }
    }
    return 0;
}

static PyObject *
unpack_typevartuples(PyObject *params)
{
    assert(PyTuple_Check(params));
    // TypeVarTuple must be unpacked when passed to Generic, so we do that here.
    if (contains_typevartuple((PyTupleObject *)params)) {
        Py_ssize_t n = PyTuple_GET_SIZE(params);
        PyObject *new_params = PyTuple_New(n);
        PyTypeObject *tp = PyInterpreterState_Get()->cached_objects.typevartuple_type;
        for (Py_ssize_t i = 0; i < n; i++) {
            PyObject *param = PyTuple_GET_ITEM(params, i);
            if (Py_IS_TYPE(param, tp)) {
                PyObject *unpacked = typevartuple_unpack(param);
                if (unpacked == NULL) {
                    Py_DECREF(new_params);
                    return NULL;
                }
                PyTuple_SET_ITEM(new_params, i, unpacked);
            }
            else {
                PyTuple_SET_ITEM(new_params, i, Py_NewRef(param));
            }
        }
        return new_params;
    }
    else {
        return Py_NewRef(params);
    }
}

static void
typevar_dealloc(PyObject *self)
{
    PyTypeObject *tp = Py_TYPE(self);
    typevarobject *tv = (typevarobject *)self;

    _PyObject_GC_UNTRACK(self);

    free((void *)tv->name);
    Py_XDECREF(tv->bound);
    Py_XDECREF(tv->evaluate_bound);
    Py_XDECREF(tv->constraints);
    Py_XDECREF(tv->evaluate_constraints);
    Py_XDECREF(tv->default_);
    Py_XDECREF(tv->evaluate_default);
    _PyObject_ClearManagedDict(self);

    Py_TYPE(self)->tp_free(self);
    Py_DECREF(tp);
}

static int
typevar_traverse(PyObject *self, visitproc visit, void *arg)
{
    Py_VISIT(Py_TYPE(self));
    typevarobject *tv = (typevarobject *)self;
    Py_VISIT(tv->bound);
    Py_VISIT(tv->evaluate_bound);
    Py_VISIT(tv->constraints);
    Py_VISIT(tv->evaluate_constraints);
    Py_VISIT(tv->default_);
    Py_VISIT(tv->evaluate_default);
    _PyObject_VisitManagedDict(self, visit, arg);
    return 0;
}

static int
typevar_clear(typevarobject *self)
{
    Py_CLEAR(self->bound);
    Py_CLEAR(self->evaluate_bound);
    Py_CLEAR(self->constraints);
    Py_CLEAR(self->evaluate_constraints);
    Py_CLEAR(self->default_);
    Py_CLEAR(self->evaluate_default);
    _PyObject_ClearManagedDict((PyObject *)self);
    return 0;
}

static PyObject *
typevar_repr(PyObject *self)
{
    typevarobject *tv = (typevarobject *)self;

    if (tv->infer_variance) {
        return PyUnicode_FromFormat("%s", tv->name);
    }

    char variance = tv->covariant ? '+' : tv->contravariant ? '-' : '~';
    return PyUnicode_FromFormat("%c%s", variance, tv->name);
}

static PyMemberDef typevar_members[] = {
    {"__name__", T_STRING, offsetof(typevarobject, name), READONLY},
    {"__covariant__", T_BOOL, offsetof(typevarobject, covariant), READONLY},
    {"__contravariant__", T_BOOL, offsetof(typevarobject, contravariant), READONLY},
    {"__infer_variance__", T_BOOL, offsetof(typevarobject, infer_variance), READONLY},
    {0}
};

static PyObject *
typevar_bound(typevarobject *self, void *unused)
{
    if (self->bound != NULL) {
        return Py_NewRef(self->bound);
    }
    if (self->evaluate_bound == NULL) {
        Py_RETURN_NONE;
    }
    PyObject *bound = PyObject_CallNoArgs(self->evaluate_bound);
    self->bound = Py_XNewRef(bound);
    return bound;
}

static PyObject *
typevar_default(typevarobject *self, void *unused)
{
    if (self->default_ != NULL) {
        return Py_NewRef(self->default_);
    }
    if (self->evaluate_default == NULL) {
        Py_RETURN_NONE;
    }
    PyObject *default_ = PyObject_CallNoArgs(self->evaluate_default);
    if (Py_IsNone(default_)) {
        default_ = (PyObject *)Py_TYPE(default_);
    }
    self->default_ = Py_XNewRef(default_);
    return default_;
}

static PyObject *
typevar_constraints(typevarobject *self, void *unused)
{
    if (self->constraints != NULL) {
        return Py_NewRef(self->constraints);
    }
    if (self->evaluate_constraints == NULL) {
        Py_RETURN_NONE;
    }
    PyObject *constraints = PyObject_CallNoArgs(self->evaluate_constraints);
    self->constraints = Py_XNewRef(constraints);
    return constraints;
}

static PyGetSetDef typevar_getset[] = {
    {"__bound__", (getter)typevar_bound, NULL, NULL, NULL},
    {"__constraints__", (getter)typevar_constraints, NULL, NULL, NULL},
    {"__default__", (getter)typevar_default, NULL, NULL, NULL},
    {0}
};

static typevarobject *
typevar_alloc(const char *name, PyObject *bound, PyObject *evaluate_bound,
              PyObject *constraints, PyObject *evaluate_constraints,
              PyObject *default_,
              bool covariant, bool contravariant, bool infer_variance,
              PyObject *module)
{

    char *owned_name = strdup(name);
    if (owned_name == NULL) {
        PyErr_NoMemory();
        return NULL;
    }

    PyTypeObject *tp = PyInterpreterState_Get()->cached_objects.typevar_type;
    assert(tp != NULL);
    typevarobject *tv = PyObject_GC_New(typevarobject, tp);
    if (tv == NULL) {
        free((void *)owned_name);
        return NULL;
    }

    tv->name = owned_name;

    tv->bound = Py_XNewRef(bound);
    tv->evaluate_bound = Py_XNewRef(evaluate_bound);
    tv->constraints = Py_XNewRef(constraints);
    tv->evaluate_constraints = Py_XNewRef(evaluate_constraints);
    tv->default_ = Py_XNewRef(default_);
    tv->evaluate_default = NULL;

    tv->covariant = covariant;
    tv->contravariant = contravariant;
    tv->infer_variance = infer_variance;
    _PyObject_GC_TRACK(tv);

    if (module != NULL) {
        if (PyObject_SetAttrString((PyObject *)tv, "__module__", module) < 0) {
            Py_DECREF(tv);
            return NULL;
        }
    }

    return tv;
}

/*[clinic input]
@classmethod
typevar.__new__ as typevar_new

    name: str
    *constraints: object
    *
    bound: object = None
    default as default_: object(c_default=NULL) = None
    covariant: bool = False
    contravariant: bool = False
    infer_variance: bool = False

Create a TypeVar.
[clinic start generated code]*/

static PyObject *
typevar_new_impl(PyTypeObject *type, const char *name, PyObject *constraints,
                 PyObject *bound, PyObject *default_, int covariant,
                 int contravariant, int infer_variance)
/*[clinic end generated code: output=db242a7539daf816 input=6d9e325136ee5408]*/
{
    if (covariant && contravariant) {
        PyErr_SetString(PyExc_ValueError,
                        "Bivariant types are not supported.");
        return NULL;
    }

    if (infer_variance && (covariant || contravariant)) {
        PyErr_SetString(PyExc_ValueError,
                        "Variance cannot be specified with infer_variance.");
        return NULL;
    }

    if (Py_IsNone(bound)) {
        bound = NULL;
    }
    if (bound != NULL) {
        bound = type_check(bound);
        if (bound == NULL) {
            return NULL;
        }
    }

    if (!PyTuple_CheckExact(constraints)) {
        PyErr_SetString(PyExc_TypeError,
                        "constraints must be a tuple");
        return NULL;
    }
    Py_ssize_t n_constraints = PyTuple_GET_SIZE(constraints);
    if (n_constraints == 1) {
        PyErr_SetString(PyExc_TypeError,
                        "A single constraint is not allowed");
        Py_XDECREF(bound);
        return NULL;
    } else if (n_constraints == 0) {
        constraints = NULL;
    } else if (bound != NULL) {
        PyErr_SetString(PyExc_TypeError,
                        "Constraints cannot be combined with bound=...");
        Py_XDECREF(bound);
        return NULL;
    }
    PyObject *module = caller();
    if (module == NULL) {
        Py_XDECREF(bound);
        return NULL;
    }
    if (Py_IsNone(default_)) {
        default_ = (PyObject *)Py_TYPE(default_);
    }

    PyObject *tv = (PyObject *)typevar_alloc(name, bound, NULL,
                                             constraints, NULL,
                                             default_,
                                             covariant, contravariant,
                                             infer_variance, module);
    Py_XDECREF(bound);
    Py_XDECREF(module);
    return tv;
}

/*[clinic input]
typevar.__typing_subst__ as typevar_typing_subst

    arg: object

[clinic start generated code]*/

static PyObject *
typevar_typing_subst_impl(typevarobject *self, PyObject *arg)
/*[clinic end generated code: output=c76ced134ed8f4e1 input=6b70a4bb2da838de]*/
{
    PyObject *args = PyTuple_Pack(2, self, arg);
    if (args == NULL) {
        return NULL;
    }
    PyObject *result = call_typing_func_object("_typevar_subst", args);
    Py_DECREF(args);
    return result;
}

/*[clinic input]
typevar.__reduce__ as typevar_reduce

[clinic start generated code]*/

static PyObject *
typevar_reduce_impl(typevarobject *self)
/*[clinic end generated code: output=02e5c55d7cf8a08f input=de76bc95f04fb9ff]*/
{
    return PyUnicode_FromString(self->name);
}

static PyObject *
typevar_mro_entries(PyObject *self, PyObject *args)
{
    PyErr_SetString(PyExc_TypeError,
                    "Cannot subclass an instance of TypeVar");
    return NULL;
}

static PyMethodDef typevar_methods[] = {
    TYPEVAR_TYPING_SUBST_METHODDEF
    TYPEVAR_REDUCE_METHODDEF
    {"__mro_entries__", typevar_mro_entries, METH_O},
    {0}
};

PyDoc_STRVAR(typevar_doc,
"Type variable.\n\
\n\
Usage::\n\
\n\
  T = TypeVar('T')  # Can be anything\n\
  A = TypeVar('A', str, bytes)  # Must be str or bytes\n\
\n\
Type variables exist primarily for the benefit of static type\n\
checkers.  They serve as the parameters for generic types as well\n\
as for generic function definitions.  See class Generic for more\n\
information on generic types.  Generic functions work as follows:\n\
\n\
  def repeat(x: T, n: int) -> List[T]:\n\
      '''Return a list containing n references to x.'''\n\
      return [x]*n\n\
\n\
  def longest(x: A, y: A) -> A:\n\
      '''Return the longest of two strings.'''\n\
      return x if len(x) >= len(y) else y\n\
\n\
The latter example's signature is essentially the overloading\n\
of (str, str) -> str and (bytes, bytes) -> bytes.  Also note\n\
that if the arguments are instances of some subclass of str,\n\
the return type is still plain str.\n\
\n\
At runtime, isinstance(x, T) and issubclass(C, T) will raise TypeError.\n\
\n\
Type variables defined with covariant=True or contravariant=True\n\
can be used to declare covariant or contravariant generic types.\n\
See PEP 484 for more details. By default generic types are invariant\n\
in all type variables.\n\
\n\
Type variables can be introspected. e.g.:\n\
\n\
  T.__name__ == 'T'\n\
  T.__constraints__ == ()\n\
  T.__covariant__ == False\n\
  T.__contravariant__ = False\n\
  A.__constraints__ == (str, bytes)\n\
\n\
Note that only type variables defined in global scope can be pickled.\n\
");

static PyType_Slot typevar_slots[] = {
    {Py_tp_doc, (void *)typevar_doc},
    {Py_tp_methods, typevar_methods},
    {Py_nb_or, make_union},
    {Py_tp_new, typevar_new},
    {Py_tp_dealloc, typevar_dealloc},
    {Py_tp_alloc, PyType_GenericAlloc},
    {Py_tp_free, PyObject_GC_Del},
    {Py_tp_traverse, typevar_traverse},
    {Py_tp_clear, typevar_clear},
    {Py_tp_repr, typevar_repr},
    {Py_tp_members, typevar_members},
    {Py_tp_getset, typevar_getset},
    {0, NULL},
};

PyType_Spec typevar_spec = {
    .name = "typing.TypeVar",
    .basicsize = sizeof(typevarobject),
    .flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_HAVE_GC | Py_TPFLAGS_IMMUTABLETYPE
        | Py_TPFLAGS_MANAGED_DICT,
    .slots = typevar_slots,
};

typedef struct {
    PyObject_HEAD
    PyObject *__origin__;
} paramspecattrobject;

static void
paramspecattr_dealloc(PyObject *self)
{
    PyTypeObject *tp = Py_TYPE(self);
    paramspecattrobject *psa = (paramspecattrobject *)self;

    _PyObject_GC_UNTRACK(self);

    Py_XDECREF(psa->__origin__);

    Py_TYPE(self)->tp_free(self);
    Py_DECREF(tp);
}

static int
paramspecattr_traverse(PyObject *self, visitproc visit, void *arg)
{
    paramspecattrobject *psa = (paramspecattrobject *)self;
    Py_VISIT(psa->__origin__);
    return 0;
}

static int
paramspecattr_clear(paramspecattrobject *self)
{
    Py_CLEAR(self->__origin__);
    return 0;
}

static PyObject *
paramspecattr_richcompare(PyObject *a, PyObject *b, int op)
{
    if (!Py_IS_TYPE(a, Py_TYPE(b))) {
        Py_RETURN_NOTIMPLEMENTED;
    }
    if (op != Py_EQ && op != Py_NE) {
        Py_RETURN_NOTIMPLEMENTED;
    }
    return PyObject_RichCompare(
        ((paramspecattrobject *)a)->__origin__,
        ((paramspecattrobject *)b)->__origin__,
        op
    );
}

static PyMemberDef paramspecattr_members[] = {
    {"__origin__", T_OBJECT, offsetof(paramspecattrobject, __origin__), READONLY},
    {0}
};

static paramspecattrobject *
paramspecattr_new(PyTypeObject *tp, PyObject *origin)
{
    paramspecattrobject *psa = PyObject_GC_New(paramspecattrobject, tp);
    if (psa == NULL) {
        return NULL;
    }
    psa->__origin__ = Py_NewRef(origin);
    _PyObject_GC_TRACK(psa);
    return psa;
}

static PyObject *
paramspecargs_repr(PyObject *self)
{
    paramspecattrobject *psa = (paramspecattrobject *)self;

    PyTypeObject *tp = PyInterpreterState_Get()->cached_objects.paramspec_type;
    if (Py_IS_TYPE(psa->__origin__, tp)) {
        return PyUnicode_FromFormat("%s.args",
            ((paramspecobject *)psa->__origin__)->name);
    }
    return PyUnicode_FromFormat("%R.args", psa->__origin__);
}


/*[clinic input]
@classmethod
paramspecargs.__new__ as paramspecargs_new

    origin: object

Create a ParamSpecArgs object.
[clinic start generated code]*/

static PyObject *
paramspecargs_new_impl(PyTypeObject *type, PyObject *origin)
/*[clinic end generated code: output=9a1463dc8942fe4e input=3596a0bb6183c208]*/
{
    return (PyObject *)paramspecattr_new(type, origin);
}

static PyObject *
paramspecargs_mro_entries(PyObject *self, PyObject *args)
{
    PyErr_SetString(PyExc_TypeError,
                    "Cannot subclass an instance of ParamSpecArgs");
    return NULL;
}

static PyMethodDef paramspecargs_methods[] = {
    {"__mro_entries__", paramspecargs_mro_entries, METH_O},
    {0}
};

PyDoc_STRVAR(paramspecargs_doc,
"The args for a ParamSpec object.\n\
\n\
Given a ParamSpec object P, P.args is an instance of ParamSpecArgs.\n\
\n\
ParamSpecArgs objects have a reference back to their ParamSpec:\n\
\n\
    P.args.__origin__ is P\n\
\n\
This type is meant for runtime introspection and has no special meaning to\n\
static type checkers.\n\
");

static PyType_Slot paramspecargs_slots[] = {
    {Py_tp_doc, (void *)paramspecargs_doc},
    {Py_tp_methods, paramspecargs_methods},
    {Py_tp_new, paramspecargs_new},
    {Py_tp_dealloc, paramspecattr_dealloc},
    {Py_tp_alloc, PyType_GenericAlloc},
    {Py_tp_free, PyObject_GC_Del},
    {Py_tp_traverse, paramspecattr_traverse},
    {Py_tp_clear, (inquiry)paramspecattr_clear},
    {Py_tp_repr, paramspecargs_repr},
    {Py_tp_members, paramspecattr_members},
    {Py_tp_richcompare, paramspecattr_richcompare},
    {0, NULL},
};

PyType_Spec paramspecargs_spec = {
    .name = "typing.ParamSpecArgs",
    .basicsize = sizeof(paramspecattrobject),
    .flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_HAVE_GC | Py_TPFLAGS_IMMUTABLETYPE,
    .slots = paramspecargs_slots,
};

static PyObject *
paramspeckwargs_repr(PyObject *self)
{
    paramspecattrobject *psk = (paramspecattrobject *)self;

    PyTypeObject *tp = PyInterpreterState_Get()->cached_objects.paramspec_type;
    if (Py_IS_TYPE(psk->__origin__, tp)) {
        return PyUnicode_FromFormat("%s.kwargs",
            ((paramspecobject *)psk->__origin__)->name);
    }
    return PyUnicode_FromFormat("%R.kwargs", psk->__origin__);
}

/*[clinic input]
@classmethod
paramspeckwargs.__new__ as paramspeckwargs_new

    origin: object

Create a ParamSpecKwargs object.
[clinic start generated code]*/

static PyObject *
paramspeckwargs_new_impl(PyTypeObject *type, PyObject *origin)
/*[clinic end generated code: output=277b11967ebaf4ab input=981bca9b0cf9e40a]*/
{
    return (PyObject *)paramspecattr_new(type, origin);
}

static PyObject *
paramspeckwargs_mro_entries(PyObject *self, PyObject *args)
{
    PyErr_SetString(PyExc_TypeError,
                    "Cannot subclass an instance of ParamSpecKwargs");
    return NULL;
}

static PyMethodDef paramspeckwargs_methods[] = {
    {"__mro_entries__", paramspeckwargs_mro_entries, METH_O},
    {0}
};

PyDoc_STRVAR(paramspeckwargs_doc,
"The kwargs for a ParamSpec object.\n\
\n\
Given a ParamSpec object P, P.kwargs is an instance of ParamSpecKwargs.\n\
\n\
ParamSpecKwargs objects have a reference back to their ParamSpec:\n\
\n\
    P.kwargs.__origin__ is P\n\
\n\
This type is meant for runtime introspection and has no special meaning to\n\
static type checkers.\n\
");

static PyType_Slot paramspeckwargs_slots[] = {
    {Py_tp_doc, (void *)paramspeckwargs_doc},
    {Py_tp_methods, paramspeckwargs_methods},
    {Py_tp_new, paramspeckwargs_new},
    {Py_tp_dealloc, paramspecattr_dealloc},
    {Py_tp_alloc, PyType_GenericAlloc},
    {Py_tp_free, PyObject_GC_Del},
    {Py_tp_traverse, paramspecattr_traverse},
    {Py_tp_clear, (inquiry)paramspecattr_clear},
    {Py_tp_repr, paramspeckwargs_repr},
    {Py_tp_members, paramspecattr_members},
    {Py_tp_richcompare, paramspecattr_richcompare},
    {0, NULL},
};

PyType_Spec paramspeckwargs_spec = {
    .name = "typing.ParamSpecKwargs",
    .basicsize = sizeof(paramspecattrobject),
    .flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_HAVE_GC | Py_TPFLAGS_IMMUTABLETYPE,
    .slots = paramspeckwargs_slots,
};

static void
paramspec_dealloc(PyObject *self)
{
    PyTypeObject *tp = Py_TYPE(self);
    paramspecobject *ps = (paramspecobject *)self;

    _PyObject_GC_UNTRACK(self);

    free((void *)ps->name);
    Py_XDECREF(ps->bound);
    Py_XDECREF(ps->default_);
    Py_XDECREF(ps->evaluate_default);
    _PyObject_ClearManagedDict(self);

    Py_TYPE(self)->tp_free(self);
    Py_DECREF(tp);
}

static int
paramspec_traverse(PyObject *self, visitproc visit, void *arg)
{
    Py_VISIT(Py_TYPE(self));
    paramspecobject *ps = (paramspecobject *)self;
    Py_VISIT(ps->bound);
    Py_VISIT(ps->default_);
    Py_VISIT(ps->evaluate_default);
    _PyObject_VisitManagedDict(self, visit, arg);
    return 0;
}

static int
paramspec_clear(paramspecobject *self)
{
    Py_CLEAR(self->bound);
    Py_CLEAR(self->default_);
    Py_CLEAR(self->evaluate_default);
    _PyObject_ClearManagedDict((PyObject *)self);
    return 0;
}

static PyObject *
paramspec_repr(PyObject *self)
{
    paramspecobject *ps = (paramspecobject *)self;

    if (ps->infer_variance) {
        return PyUnicode_FromFormat("%s", ps->name);
    }

    char variance = ps->covariant ? '+' : ps->contravariant ? '-' : '~';
    return PyUnicode_FromFormat("%c%s", variance, ps->name);
}

static PyMemberDef paramspec_members[] = {
    {"__name__", T_STRING, offsetof(paramspecobject, name), READONLY},
    {"__bound__", T_OBJECT, offsetof(paramspecobject, bound), READONLY},
    {"__covariant__", T_BOOL, offsetof(paramspecobject, covariant), READONLY},
    {"__contravariant__", T_BOOL, offsetof(paramspecobject, contravariant), READONLY},
    {"__infer_variance__", T_BOOL, offsetof(paramspecobject, infer_variance), READONLY},
    {0}
};

static PyObject *
paramspec_args(PyObject *self, void *unused)
{
    PyTypeObject *tp = PyInterpreterState_Get()->cached_objects.paramspecargs_type;
    return (PyObject *)paramspecattr_new(tp, self);
}

static PyObject *
paramspec_kwargs(PyObject *self, void *unused)
{
    PyTypeObject *tp = PyInterpreterState_Get()->cached_objects.paramspeckwargs_type;
    return (PyObject *)paramspecattr_new(tp, self);
}

static PyObject *
paramspec_default(paramspecobject *self, void *unused)
{
    if (self->default_ != NULL) {
        return Py_NewRef(self->default_);
    }
    if (self->evaluate_default == NULL) {
        Py_RETURN_NONE;
    }
    PyObject *default_ = PyObject_CallNoArgs(self->evaluate_default);
    if (Py_IsNone(default_)) {
        default_ = Py_XNewRef(Py_TYPE(default_));
    }
    self->default_ = Py_XNewRef(default_);
    return default_;
}

static PyGetSetDef paramspec_getset[] = {
    {"args", (getter)paramspec_args, NULL, "Represents positional arguments.", NULL},
    {"kwargs", (getter)paramspec_kwargs, NULL, "Represents keyword arguments.", NULL},
    {"__default__", (getter)paramspec_default, NULL, "The default value for this ParamSpec.", NULL},
    {0},
};

static paramspecobject *
paramspec_alloc(const char *name, PyObject *bound, PyObject *default_, bool covariant,
                bool contravariant, bool infer_variance, PyObject *module)
{
    char *owned_name = strdup(name);
    if (owned_name == NULL) {
        PyErr_NoMemory();
        return NULL;
    }
    PyTypeObject *tp = PyInterpreterState_Get()->cached_objects.paramspec_type;
    paramspecobject *ps = PyObject_GC_New(paramspecobject, tp);
    if (ps == NULL) {
        free((void *)owned_name);
        return NULL;
    }
    ps->name = owned_name;
    ps->bound = Py_XNewRef(bound);
    ps->covariant = covariant;
    ps->contravariant = contravariant;
    ps->infer_variance = infer_variance;
    ps->default_ = default_;
    ps->evaluate_default = NULL;
    _PyObject_GC_TRACK(ps);
    if (module != NULL) {
        if (PyObject_SetAttrString((PyObject *)ps, "__module__", module) < 0) {
            Py_DECREF(ps);
            return NULL;
        }
    }
    return ps;
}

/*[clinic input]
@classmethod
paramspec.__new__ as paramspec_new

    name: str
    *
    bound: object = None
    default as default_: object(c_default="NULL") = None
    covariant: bool = False
    contravariant: bool = False
    infer_variance: bool = False

Create a ParamSpec object.
[clinic start generated code]*/

static PyObject *
paramspec_new_impl(PyTypeObject *type, const char *name, PyObject *bound,
                   PyObject *default_, int covariant, int contravariant,
                   int infer_variance)
/*[clinic end generated code: output=ac8d566efe6b15ac input=f87b9c7ab6bc58fe]*/
{
    if (covariant && contravariant) {
        PyErr_SetString(PyExc_ValueError, "Bivariant types are not supported.");
        return NULL;
    }
    if (infer_variance && (covariant || contravariant)) {
        PyErr_SetString(PyExc_ValueError, "Variance cannot be specified with infer_variance.");
        return NULL;
    }
    if (bound != NULL) {
        bound = type_check(bound);
        if (bound == NULL) {
            return NULL;
        }
    }
    PyObject *module = caller();
    if (module == NULL) {
        Py_XDECREF(bound);
        return NULL;
    }
    if (Py_IsNone(default_)) {
        default_ = Py_NewRef(Py_TYPE(default_));
    }
    PyObject *ps = (PyObject *)paramspec_alloc(
        name, bound, default_, covariant, contravariant, infer_variance, module);
    Py_XDECREF(bound);
    Py_DECREF(module);
    return ps;
}


/*[clinic input]
paramspec.__typing_subst__ as paramspec_typing_subst

    arg: object

[clinic start generated code]*/

static PyObject *
paramspec_typing_subst_impl(paramspecobject *self, PyObject *arg)
/*[clinic end generated code: output=803e1ade3f13b57d input=4e0005d24023e896]*/
{
    PyObject *args = PyTuple_Pack(2, self, arg);
    if (args == NULL) {
        return NULL;
    }
    PyObject *result = call_typing_func_object("_paramspec_subst", args);
    Py_DECREF(args);
    return result;
}

/*[clinic input]
paramspec.__typing_prepare_subst__ as paramspec_typing_prepare_subst

    alias: object
    args: object

[clinic start generated code]*/

static PyObject *
paramspec_typing_prepare_subst_impl(paramspecobject *self, PyObject *alias,
                                    PyObject *args)
/*[clinic end generated code: output=95449d630a2adb9a input=4375e2ffcb2ad635]*/
{
    PyObject *args_tuple = PyTuple_Pack(3, self, alias, args);
    if (args_tuple == NULL) {
        return NULL;
    }
    PyObject *result = call_typing_func_object("_paramspec_prepare_subst", args_tuple);
    Py_DECREF(args_tuple);
    return result;
}

/*[clinic input]
paramspec.__reduce__ as paramspec_reduce

[clinic start generated code]*/

static PyObject *
paramspec_reduce_impl(paramspecobject *self)
/*[clinic end generated code: output=b83398674416db27 input=5bf349f0d5dd426c]*/
{
    return PyUnicode_FromString(self->name);
}

static PyObject *
paramspec_mro_entries(PyObject *self, PyObject *args)
{
    PyErr_SetString(PyExc_TypeError,
                    "Cannot subclass an instance of ParamSpec");
    return NULL;
}

static PyMethodDef paramspec_methods[] = {
    PARAMSPEC_TYPING_SUBST_METHODDEF
    PARAMSPEC_TYPING_PREPARE_SUBST_METHODDEF
    PARAMSPEC_REDUCE_METHODDEF
    {"__mro_entries__", paramspec_mro_entries, METH_O},
    {0}
};

PyDoc_STRVAR(paramspec_doc,
"Parameter specification variable.\n\
\n\
Usage::\n\
\n\
    P = ParamSpec('P')\n\
\n\
Parameter specification variables exist primarily for the benefit of static\n\
type checkers.  They are used to forward the parameter types of one\n\
callable to another callable, a pattern commonly found in higher order\n\
functions and decorators.  They are only valid when used in ``Concatenate``,\n\
or as the first argument to ``Callable``, or as parameters for user-defined\n\
Generics.  See class Generic for more information on generic types.  An\n\
example for annotating a decorator::\n\
\n\
    T = TypeVar('T')\n\
    P = ParamSpec('P')\n\
\n\
    def add_logging(f: Callable[P, T]) -> Callable[P, T]:\n\
        '''A type-safe decorator to add logging to a function.'''\n\
        def inner(*args: P.args, **kwargs: P.kwargs) -> T:\n\
            logging.info(f'{f.__name__} was called')\n\
            return f(*args, **kwargs)\n\
        return inner\n\
\n\
    @add_logging\n\
    def add_two(x: float, y: float) -> float:\n\
        '''Add two numbers together.'''\n\
        return x + y\n\
\n\
Parameter specification variables defined with covariant=True or\n\
contravariant=True can be used to declare covariant or contravariant\n\
generic types.  These keyword arguments are valid, but their actual semantics\n\
are yet to be decided.  See PEP 612 for details.\n\
\n\
Parameter specification variables can be introspected. e.g.:\n\
\n\
    P.__name__ == 'P'\n\
    P.__bound__ == None\n\
    P.__covariant__ == False\n\
    P.__contravariant__ == False\n\
\n\
Note that only parameter specification variables defined in global scope can\n\
be pickled.\n\
");

static PyType_Slot paramspec_slots[] = {
    {Py_tp_doc, (void *)paramspec_doc},
    {Py_tp_members, paramspec_members},
    {Py_tp_methods, paramspec_methods},
    {Py_tp_getset, paramspec_getset},
    // Unions of ParamSpecs have no defined meaning, but they were allowed
    // by the Python implementation, so we allow them here too.
    {Py_nb_or, make_union},
    {Py_tp_new, paramspec_new},
    {Py_tp_dealloc, paramspec_dealloc},
    {Py_tp_alloc, PyType_GenericAlloc},
    {Py_tp_free, PyObject_GC_Del},
    {Py_tp_traverse, paramspec_traverse},
    {Py_tp_clear, paramspec_clear},
    {Py_tp_repr, paramspec_repr},
    {0, 0},
};

PyType_Spec paramspec_spec = {
    .name = "typing.ParamSpec",
    .basicsize = sizeof(paramspecobject),
    .flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_HAVE_GC | Py_TPFLAGS_IMMUTABLETYPE
        | Py_TPFLAGS_MANAGED_DICT,
    .slots = paramspec_slots,
};

static void
typevartuple_dealloc(PyObject *self)
{
    PyTypeObject *tp = Py_TYPE(self);
    _PyObject_GC_UNTRACK(self);
    typevartupleobject *tvt = (typevartupleobject *)self;

    free((void *)tvt->name);
    Py_XDECREF(tvt->default_);
    Py_XDECREF(tvt->evaluate_default);
    _PyObject_ClearManagedDict(self);

    Py_TYPE(self)->tp_free(self);
    Py_DECREF(tp);
}

static PyObject *
typevartuple_iter(PyObject *self)
{
    PyObject *unpacked = typevartuple_unpack(self);
    if (unpacked == NULL) {
        return NULL;
    }
    PyObject *tuple = PyTuple_Pack(1, unpacked);
    if (tuple == NULL) {
        Py_DECREF(unpacked);
        return NULL;
    }
    PyObject *result = PyObject_GetIter(tuple);
    Py_DECREF(unpacked);
    Py_DECREF(tuple);
    return result;
}

static PyObject *
typevartuple_repr(PyObject *self)
{
    typevartupleobject *tvt = (typevartupleobject *)self;

    return PyUnicode_FromFormat("%s", tvt->name);
}

static PyMemberDef typevartuple_members[] = {
    {"__name__", T_STRING, offsetof(typevartupleobject, name), READONLY},
    {0}
};

static typevartupleobject *
typevartuple_alloc(const char *name, PyObject *module, PyObject *default_)
{
    char *owned_name = strdup(name);
    if (owned_name == NULL) {
        PyErr_NoMemory();
        return NULL;
    }
    PyTypeObject *tp = PyInterpreterState_Get()->cached_objects.typevartuple_type;
    typevartupleobject *tvt = PyObject_GC_New(typevartupleobject, tp);
    if (tvt == NULL) {
        free(owned_name);
        return NULL;
    }
    tvt->name = owned_name;
    tvt->default_ = Py_XNewRef(default_);
    tvt->evaluate_default = NULL;
    _PyObject_GC_TRACK(tvt);
    if (module != NULL) {
        if (PyObject_SetAttrString((PyObject *)tvt, "__module__", module) < 0) {
            Py_DECREF(tvt);
            return NULL;
        }
    }
    return tvt;
}

/*[clinic input]
@classmethod
typevartuple.__new__

    name: str
    *
    default as default_: object(c_default="NULL") = None

Create a new TypeVarTuple with the given name.
[clinic start generated code]*/

static PyObject *
typevartuple_impl(PyTypeObject *type, const char *name, PyObject *default_)
/*[clinic end generated code: output=c4c011b4c35db972 input=b09f65448cd54772]*/
{
    PyObject *module = caller();
    if (module == NULL) {
        return NULL;
    }
    if (Py_IsNone(default_)) {
        default_ = (PyObject *)Py_TYPE(default_);
    }
    PyObject *result = (PyObject *)typevartuple_alloc(name, module, default_);
    Py_DECREF(module);
    return result;
}

/*[clinic input]
typevartuple.__typing_subst__ as typevartuple_typing_subst

    arg: object

[clinic start generated code]*/

static PyObject *
typevartuple_typing_subst_impl(typevartupleobject *self, PyObject *arg)
/*[clinic end generated code: output=814316519441cd76 input=670c4e0a36e5d8c0]*/
{
    PyErr_SetString(PyExc_TypeError, "Substitution of bare TypeVarTuple is not supported");
    return NULL;
}

/*[clinic input]
typevartuple.__typing_prepare_subst__ as typevartuple_typing_prepare_subst

    alias: object
    args: object

[clinic start generated code]*/

static PyObject *
typevartuple_typing_prepare_subst_impl(typevartupleobject *self,
                                       PyObject *alias, PyObject *args)
/*[clinic end generated code: output=ff999bc5b02036c1 input=a211b05f2eeb4306]*/
{
    PyObject *args_tuple = PyTuple_Pack(3, self, alias, args);
    if (args_tuple == NULL) {
        return NULL;
    }
    PyObject *result = call_typing_func_object("_typevartuple_prepare_subst", args_tuple);
    Py_DECREF(args_tuple);
    return result;
}

/*[clinic input]
typevartuple.__reduce__ as typevartuple_reduce

[clinic start generated code]*/

static PyObject *
typevartuple_reduce_impl(typevartupleobject *self)
/*[clinic end generated code: output=3215bc0477913d20 input=3018a4d66147e807]*/
{
    return PyUnicode_FromString(self->name);
}

static PyObject *
typevartuple_mro_entries(PyObject *self, PyObject *args)
{
    PyErr_SetString(PyExc_TypeError,
                    "Cannot subclass an instance of TypeVarTuple");
    return NULL;
}

static int
typevartuple_traverse(PyObject *self, visitproc visit, void *arg)
{
    Py_VISIT(Py_TYPE(self));
    Py_VISIT(((typevartupleobject *)self)->default_);
    Py_VISIT(((typevartupleobject *)self)->evaluate_default);
    _PyObject_VisitManagedDict(self, visit, arg);
    return 0;
}

static int
typevartuple_clear(PyObject *self)
{
    Py_CLEAR(((typevartupleobject *)self)->default_);
    Py_CLEAR(((typevartupleobject *)self)->evaluate_default);
    _PyObject_ClearManagedDict(self);
    return 0;
}

static PyObject *
typevartuple_default(typevartupleobject *self, void *unused)
{
    if (self->default_ != NULL) {
        return Py_NewRef(self->default_);
    }
    if (self->evaluate_default == NULL) {
        Py_RETURN_NONE;
    }
    PyObject *default_ = PyObject_CallNoArgs(self->evaluate_default);
    if (Py_IsNone(default_)) {
        default_ = (PyObject *)Py_TYPE(default_);
    }
    self->default_ = Py_XNewRef(default_);
    return default_;
}

static PyGetSetDef typevartuple_getset[] = {
    {"__default__", (getter)typevartuple_default, NULL, "The default value for this TypeVarTuple.", NULL},
    {0},
};

static PyMethodDef typevartuple_methods[] = {
    TYPEVARTUPLE_TYPING_SUBST_METHODDEF
    TYPEVARTUPLE_TYPING_PREPARE_SUBST_METHODDEF
    TYPEVARTUPLE_REDUCE_METHODDEF
    {"__mro_entries__", typevartuple_mro_entries, METH_O},
    {0}
};

PyDoc_STRVAR(typevartuple_doc,
"Type variable tuple.\n\
\n\
Usage:\n\
\n\
  Ts = TypeVarTuple('Ts')  # Can be given any name\n\
\n\
Just as a TypeVar (type variable) is a placeholder for a single type,\n\
a TypeVarTuple is a placeholder for an *arbitrary* number of types. For\n\
example, if we define a generic class using a TypeVarTuple:\n\
\n\
  class C(Generic[*Ts]): ...\n\
\n\
Then we can parameterize that class with an arbitrary number of type\n\
arguments:\n\
\n\
  C[int]       # Fine\n\
  C[int, str]  # Also fine\n\
  C[()]        # Even this is fine\n\
\n\
For more details, see PEP 646.\n\
\n\
Note that only TypeVarTuples defined in global scope can be pickled.\n\
");

PyType_Slot typevartuple_slots[] = {
    {Py_tp_doc, (void *)typevartuple_doc},
    {Py_tp_members, typevartuple_members},
    {Py_tp_methods, typevartuple_methods},
    {Py_tp_getset, typevartuple_getset},
    {Py_tp_new, typevartuple},
    {Py_tp_iter, typevartuple_iter},
    {Py_tp_repr, typevartuple_repr},
    {Py_tp_dealloc, typevartuple_dealloc},
    {Py_tp_alloc, PyType_GenericAlloc},
    {Py_tp_free, PyObject_GC_Del},
    {Py_tp_traverse, typevartuple_traverse},
    {Py_tp_clear, typevartuple_clear},
    {0, 0},
};

PyType_Spec typevartuple_spec = {
    .name = "typing.TypeVarTuple",
    .basicsize = sizeof(typevartupleobject),
    .flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_IMMUTABLETYPE | Py_TPFLAGS_MANAGED_DICT
        | Py_TPFLAGS_HAVE_GC,
    .slots = typevartuple_slots,
};

PyObject *
_Py_make_typevar(const char *name, PyObject *evaluate_bound, PyObject *evaluate_constraints)
{
    return (PyObject *)typevar_alloc(name, NULL, evaluate_bound, NULL, evaluate_constraints,
                                     NULL, false, false, true, NULL);
}

PyObject *
_Py_make_paramspec(PyThreadState *unused, PyObject *v)
{
    assert(PyUnicode_Check(v));
    return (PyObject *)paramspec_alloc(PyUnicode_AsUTF8(v), NULL, NULL, false, false, true, NULL);
}

PyObject *
_Py_make_typevartuple(PyThreadState *unused, PyObject *v)
{
    assert(PyUnicode_Check(v));
    return (PyObject *)typevartuple_alloc(PyUnicode_AsUTF8(v), NULL, NULL);
}

static void
typealias_dealloc(PyObject *self)
{
    PyTypeObject *tp = Py_TYPE(self);
    _PyObject_GC_UNTRACK(self);
    typealiasobject *ta = (typealiasobject *)self;
    free((void *)ta->name);
    Py_XDECREF(ta->type_params);
    Py_XDECREF(ta->compute_value);
    Py_XDECREF(ta->value);
    Py_TYPE(self)->tp_free(self);
    Py_DECREF(tp);
}

static PyObject *
typealias_evaluate(typealiasobject *ta)
{
    if (ta->value != NULL) {
        return Py_NewRef(ta->value);
    }
    PyObject *result = PyObject_CallNoArgs(ta->compute_value);
    if (result == NULL) {
        return NULL;
    }
    ta->value = Py_NewRef(result);
    return result;
}

static PyObject *
typealias_repr(PyObject *self)
{
    Py_ssize_t res = Py_ReprEnter(self);
    if (res > 0) {
        return PyUnicode_FromString("...");
    }
    else if (res < 0) {
        return NULL;
    }

    typealiasobject *ta = (typealiasobject *)self;
    PyObject *value_repr = NULL;
    PyObject *value = typealias_evaluate(ta);
    if (value == NULL) {
        PyErr_Clear();
        value_repr = PyUnicode_FromString("<evaluation failed>");
    }
    else if (PyType_Check(value) && Py_TYPE(value)->tp_repr == PyType_Type.tp_repr
             && ((PyTypeObject *)value)->tp_name != NULL) {
        value_repr = PyUnicode_FromString(((PyTypeObject *)value)->tp_name);
        Py_DECREF(value);
    }
    else {
        value_repr = PyObject_Repr(value);
        Py_DECREF(value);
        if (value_repr == NULL) {
            PyErr_Clear();
            value_repr = PyUnicode_FromString("<value repr() failed>");
        }
    }
    if (value_repr == NULL) {
        // PyUnicode_FromString failed
        Py_ReprLeave(self);
        return NULL;
    }
    PyObject *result = NULL;
    if (ta->type_params != NULL) {
        _PyUnicodeWriter writer;
        _PyUnicodeWriter_Init(&writer);
        PyTupleObject *type_params = (PyTupleObject *)ta->type_params;
        Py_ssize_t count = PyTuple_GET_SIZE(type_params);
        PyInterpreterState *interp = PyInterpreterState_Get();
        for (Py_ssize_t i = 0; i < count; i++) {
            PyObject *type_param = PyTuple_GET_ITEM(type_params, i);
            if (i > 0 && _PyUnicodeWriter_WriteASCIIString(&writer, ", ", 2) < 0) {
                _PyUnicodeWriter_Dealloc(&writer);
                goto error;
            }
            if (Py_IS_TYPE(type_param, interp->cached_objects.paramspec_type)) {
                if (_PyUnicodeWriter_WriteASCIIString(&writer, "**", 2) < 0) {
                    _PyUnicodeWriter_Dealloc(&writer);
                    goto error;
                }
            }
            else if (Py_IS_TYPE(type_param, interp->cached_objects.typevartuple_type)) {
                if (_PyUnicodeWriter_WriteASCIIString(&writer, "*", 1) < 0) {
                    _PyUnicodeWriter_Dealloc(&writer);
                    goto error;
                }
            }
            PyObject *type_param_repr = PyObject_Repr(type_param);
            if (type_param_repr == NULL) {
                _PyUnicodeWriter_Dealloc(&writer);
                goto error;
            }
            if (_PyUnicodeWriter_WriteStr(&writer, type_param_repr) < 0) {
                Py_DECREF(type_param_repr);
                _PyUnicodeWriter_Dealloc(&writer);
                goto error;
            }
            Py_DECREF(type_param_repr);
        }
        PyObject *params_repr = _PyUnicodeWriter_Finish(&writer);
        if (params_repr == NULL) {
            goto error;
        }
        result = PyUnicode_FromFormat("<type alias %s[%U]: %U>", ta->name, params_repr, value_repr);
        Py_DECREF(params_repr);
    }
    else {
        result = PyUnicode_FromFormat("<type alias %s: %U>", ta->name, value_repr);
    }
error:
    Py_ReprLeave(self);
    Py_DECREF(value_repr);
    return result;
}

static PyMemberDef typealias_members[] = {
    {"__name__", T_STRING, offsetof(typealiasobject, name), READONLY},
    {0}
};

static PyObject *
typealias_value(PyObject *self, void *unused)
{
    typealiasobject *ta = (typealiasobject *)self;
    return typealias_evaluate(ta);
}

static PyObject *
typealias_parameters(PyObject *self, void *unused)
{
    typealiasobject *ta = (typealiasobject *)self;
    if (ta->type_params == NULL) {
        return PyTuple_New(0);
    }
    return unpack_typevartuples(ta->type_params);
}

static PyObject *
typealias_type_params(PyObject *self, void *unused)
{
    typealiasobject *ta = (typealiasobject *)self;
    if (ta->type_params == NULL) {
        return PyTuple_New(0);
    }
    return Py_NewRef(ta->type_params);
}

static PyGetSetDef typealias_getset[] = {
    {"__parameters__", typealias_parameters, (setter)NULL, NULL, NULL},
    {"__type_params__", typealias_type_params, (setter)NULL, NULL, NULL},
    {"__value__", typealias_value, (setter)NULL, NULL, NULL},
    {0}
};

static typealiasobject *
typealias_alloc(const char *name, PyObject *type_params, PyObject *compute_value)
{
    char *owned_name = strdup(name);
    if (owned_name == NULL) {
        PyErr_NoMemory();
        return NULL;
    }
    PyTypeObject *tp = PyInterpreterState_Get()->cached_objects.typealias_type;
    typealiasobject *ta = PyObject_GC_New(typealiasobject, tp);
    if (ta == NULL) {
        free(owned_name);
        return NULL;
    }
    ta->name = owned_name;
    ta->type_params = Py_IsNone(type_params) ? NULL : Py_XNewRef(type_params);
    ta->compute_value = Py_NewRef(compute_value);
    ta->value = NULL;
    _PyObject_GC_TRACK(ta);
    return ta;
}

static int
typealias_traverse(typealiasobject *self, visitproc visit, void *arg)
{
    Py_VISIT(self->type_params);
    Py_VISIT(self->compute_value);
    Py_VISIT(self->value);
    return 0;
}

static int
typealias_clear(typealiasobject *self)
{
    Py_CLEAR(self->type_params);
    Py_CLEAR(self->compute_value);
    Py_CLEAR(self->value);
    return 0;
}

/*[clinic input]
typealias.__reduce__ as typealias_reduce

[clinic start generated code]*/

static PyObject *
typealias_reduce_impl(typealiasobject *self)
/*[clinic end generated code: output=913724f92ad3b39b input=4f06fbd9472ec0f1]*/
{
    return PyUnicode_FromString(self->name);
}

static PyObject *
typealias_subscript(PyObject *self, PyObject *args)
{
    if (((typealiasobject *)self)->type_params == NULL) {
        PyErr_SetString(PyExc_TypeError,
                        "Only generic type aliases are subscriptable");
        return NULL;
    }
    return Py_GenericAlias(self, args);
}

static PyMethodDef typealias_methods[] = {
    TYPEALIAS_REDUCE_METHODDEF
    {0}
};

PyDoc_STRVAR(typealias_doc,
"Type alias.\n\
\n\
Type aliases are created through the type statement:\n\
\n\
  type Alias = int\n\
");

static PyType_Slot typealias_slots[] = {
    {Py_tp_doc, (void *)typealias_doc},
    {Py_tp_members, typealias_members},
    {Py_tp_methods, typealias_methods},
    {Py_tp_getset, typealias_getset},
    {Py_mp_subscript, typealias_subscript},
    {Py_tp_dealloc, typealias_dealloc},
    {Py_tp_alloc, PyType_GenericAlloc},
    {Py_tp_free, PyObject_GC_Del},
    {Py_tp_traverse, (traverseproc)typealias_traverse},
    {Py_tp_clear, (inquiry)typealias_clear},
    {Py_tp_repr, typealias_repr},
    {Py_nb_or, _Py_union_type_or},
    {0, 0},
};

PyType_Spec typealias_spec = {
    .name = "typing.TypeAliasType",
    .basicsize = sizeof(typealiasobject),
    .flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_IMMUTABLETYPE | Py_TPFLAGS_HAVE_GC,
    .slots = typealias_slots,
};

PyObject *
_Py_make_typealias(PyThreadState* unused, PyObject *args)
{
    assert(PyTuple_Check(args));
    assert(PyTuple_GET_SIZE(args) == 3);
    PyObject *name = PyTuple_GET_ITEM(args, 0);
    assert(PyUnicode_Check(name));
    const char *name_str = PyUnicode_AsUTF8(name);
    if (name_str == NULL) {
        return NULL;
    }
    PyObject *type_params = PyTuple_GET_ITEM(args, 1);
    PyObject *compute_value = PyTuple_GET_ITEM(args, 2);
    return (PyObject *)typealias_alloc(name_str, type_params, compute_value);
}

PyDoc_STRVAR(generic_doc,
"Abstract base class for generic types.\n\
\n\
A generic type is typically declared by inheriting from\n\
this class parameterized with one or more type variables.\n\
For example, a generic mapping type might be defined as::\n\
\n\
    class Mapping(Generic[KT, VT]):\n\
        def __getitem__(self, key: KT) -> VT:\n\
            ...\n\
        # Etc.\n\
\n\
This class can then be used as follows::\n\
\n\
    def lookup_name(mapping: Mapping[KT, VT], key: KT, default: VT) -> VT:\n\
        try:\n\
            return mapping[key]\n\
        except KeyError:\n\
            return default\n\
");

PyDoc_STRVAR(generic_class_getitem_doc,
"Parameterizes a generic class.\n\
\n\
At least, parameterizing a generic class is the *main* thing this method\n\
does. For example, for some generic class `Foo`, this is called when we\n\
do `Foo[int]` - there, with `cls=Foo` and `params=int`.\n\
\n\
However, note that this method is also called when defining generic\n\
classes in the first place with `class Foo(Generic[T]): ...`.\n\
");

static PyObject *
call_typing_args_kwargs(const char *name, PyTypeObject *cls, PyObject *args, PyObject *kwargs)
{
    PyObject *typing = NULL, *func = NULL, *new_args = NULL;
    typing = PyImport_ImportModule("typing");
    if (typing == NULL) {
        goto error;
    }
    func = PyObject_GetAttrString(typing, name);
    if (func == NULL) {
        goto error;
    }
    assert(PyTuple_Check(args));
    Py_ssize_t nargs = PyTuple_GET_SIZE(args);
    new_args = PyTuple_New(nargs + 1);
    if (new_args == NULL) {
        goto error;
    }
    PyTuple_SET_ITEM(new_args, 0, Py_NewRef((PyObject *)cls));
    for (Py_ssize_t i = 0; i < nargs; i++) {
        PyObject *arg = PyTuple_GET_ITEM(args, i);
        PyTuple_SET_ITEM(new_args, i + 1, Py_NewRef(arg));
    }
    PyObject *result = PyObject_Call(func, new_args, kwargs);
    Py_DECREF(typing);
    Py_DECREF(func);
    Py_DECREF(new_args);
    return result;
error:
    Py_XDECREF(typing);
    Py_XDECREF(func);
    Py_XDECREF(new_args);
    return NULL;
}

static PyObject *
generic_init_subclass(PyTypeObject *cls, PyObject *args, PyObject *kwargs)
{
    return call_typing_args_kwargs("_generic_init_subclass", cls, args, kwargs);
}

static PyObject *
generic_class_getitem(PyTypeObject *cls, PyObject *args, PyObject *kwargs)
{
    return call_typing_args_kwargs("_generic_class_getitem", cls, args, kwargs);
}

PyObject *
_Py_subscript_generic(PyThreadState* unused, PyObject *params)
{
    params = unpack_typevartuples(params);

    PyInterpreterState *interp = PyInterpreterState_Get();
    if (interp->cached_objects.generic_type == NULL) {
        PyErr_SetString(PyExc_SystemError, "Cannot find Generic type");
        return NULL;
    }
    PyObject *args = PyTuple_Pack(2, interp->cached_objects.generic_type, params);
    if (args == NULL) {
        return NULL;
    }
    PyObject *result = call_typing_func_object("_generic_class_getitem", args);
    Py_DECREF(args);
    Py_DECREF(params);
    return result;
}

static PyMethodDef generic_methods[] = {
    {"__class_getitem__", (PyCFunction)(void (*)(void))generic_class_getitem,
     METH_VARARGS | METH_KEYWORDS | METH_CLASS,
     generic_class_getitem_doc},
    {"__init_subclass__", (PyCFunction)(void (*)(void))generic_init_subclass,
     METH_VARARGS | METH_KEYWORDS | METH_CLASS,
     PyDoc_STR("Function to initialize subclasses.")},
    {NULL} /* Sentinel */
};

static void
generic_dealloc(PyObject *self)
{
    PyTypeObject *tp = Py_TYPE(self);
    _PyObject_GC_UNTRACK(self);
    Py_TYPE(self)->tp_free(self);
    Py_DECREF(tp);
}

static int
generic_traverse(PyObject *self, visitproc visit, void *arg)
{
    Py_VISIT(Py_TYPE(self));
    return 0;
}

static PyType_Slot generic_slots[] = {
    {Py_tp_doc, (void *)generic_doc},
    {Py_tp_methods, generic_methods},
    {Py_tp_dealloc, generic_dealloc},
    {Py_tp_alloc, PyType_GenericAlloc},
    {Py_tp_free, PyObject_GC_Del},
    {Py_tp_traverse, generic_traverse},
    {0, NULL},
};

PyType_Spec generic_spec = {
    .name = "typing.Generic",
    .basicsize = sizeof(PyObject),
    .flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE | Py_TPFLAGS_HAVE_GC,
    .slots = generic_slots,
};

int _Py_initialize_generic(PyInterpreterState *interp)
{
#define MAKE_TYPE(name) \
    do { \
        PyTypeObject *name ## _type = (PyTypeObject *)PyType_FromSpec(&name ## _spec); \
        if (name ## _type == NULL) { \
            return -1; \
        } \
        interp->cached_objects.name ## _type = name ## _type; \
    } while(0)

    MAKE_TYPE(generic);
    MAKE_TYPE(typevar);
    MAKE_TYPE(typevartuple);
    MAKE_TYPE(paramspec);
    MAKE_TYPE(paramspecargs);
    MAKE_TYPE(paramspeckwargs);
    MAKE_TYPE(typealias);
#undef MAKE_TYPE
    return 0;
}

void _Py_clear_generic_types(PyInterpreterState *interp)
{
    Py_CLEAR(interp->cached_objects.generic_type);
    Py_CLEAR(interp->cached_objects.typevar_type);
    Py_CLEAR(interp->cached_objects.typevartuple_type);
    Py_CLEAR(interp->cached_objects.paramspec_type);
    Py_CLEAR(interp->cached_objects.paramspecargs_type);
    Py_CLEAR(interp->cached_objects.paramspeckwargs_type);
    Py_CLEAR(interp->cached_objects.typealias_type);
}

PyObject *
_Py_set_typeparam_default(PyThreadState *ts, PyObject *typeparam, PyObject *evaluate_default)
{
    if (Py_IS_TYPE(typeparam, ts->interp->cached_objects.typevar_type)) {
        Py_XSETREF(((typevarobject *)typeparam)->evaluate_default, Py_NewRef(evaluate_default));
        return Py_NewRef(typeparam);
    }
    else if (Py_IS_TYPE(typeparam, ts->interp->cached_objects.paramspec_type)) {
        Py_XSETREF(((paramspecobject *)typeparam)->evaluate_default, Py_NewRef(evaluate_default));
        return Py_NewRef(typeparam);
    }
    else if (Py_IS_TYPE(typeparam, ts->interp->cached_objects.typevartuple_type)) {
        Py_XSETREF(((typevartupleobject *)typeparam)->evaluate_default, Py_NewRef(evaluate_default));
        return Py_NewRef(typeparam);
    }
    else {
        PyErr_Format(PyExc_TypeError, "Expected a type param, got %R", typeparam);
        return NULL;
    }
}

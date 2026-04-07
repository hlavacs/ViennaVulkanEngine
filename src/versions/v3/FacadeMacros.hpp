#pragma once

#define VVE_V3_DEFINE_FACADE_CTOR(Facade, Implementation, DeclArgs, CallArgs)                                        \
   template <> Facade<Implementation>::Facade DeclArgs : implementation_(detail::makeImplementation<Implementation> CallArgs) {}

#define VVE_V3_DEFINE_FACADE_METHOD(Facade, Implementation, Method, DeclArgs, CallArgs, Qualifiers, ...)             \
   template <> __VA_ARGS__ Facade<Implementation>::Method DeclArgs Qualifiers { return implementation_->Method CallArgs; }

#define VVE_V3_DEFINE_FACADE_VOID_METHOD(Facade, Implementation, Method, DeclArgs, CallArgs, Qualifiers)             \
   template <> void Facade<Implementation>::Method DeclArgs Qualifiers { implementation_->Method CallArgs; }

/*
 * Game Server
 * Copyright (C) 2010 Miroslav 'Wayland' Kudrnac
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#ifndef _CALLBACK_H
#define _CALLBACK_H

class CallbackBase {
public:
	virtual void execute() = 0;
	virtual ~CallbackBase() {};
};

class CallbackFP {
	typedef void(*callback)();
	callback mycallback;
public:
	CallbackFP(callback cb) : mycallback(cb) {}
	void operator()();
	void execute();
	void set(callback cb) { mycallback = cb; }
	CallbackFP* Create();
};

template <class Class>
class CallbackP0 : public CallbackBase
{
public:

	typedef void (Class::*Method)();
	CallbackP0(Class* _class_instance, Method _method)
	{
	   m_obj = _class_instance;
	   m_func = _method;
	};
	void operator()() { return (m_obj->*m_func)(); }
	void execute() { return operator()(); }

//private:

	Class*  m_obj;
	Method  m_func;
};

template <class Class, typename P1>
class CallbackP1 : public CallbackBase
{
public:

	typedef void (Class::*Method)(P1);
	CallbackP1(Class* _class_instance, Method _method, P1 p1)
	{
	   _obj = _class_instance;
	   _func = _method;
	   _p1 = p1;
	};

	void operator()() { return (_obj->*_func)(_p1); }
	void execute() { return operator()(); }

//private:

	Class*  _obj;
	Method  _func;
	P1 _p1;
};

template <class Class, typename P1, typename P2>
class CallbackP2 : public CallbackBase
{
public:

	typedef void (Class::*Method)(P1, P2);
	CallbackP2(Class* _class_instance, Method _method, P1 p1, P2 p2)
	{
	   _obj = _class_instance;
	   _func = _method;
	   _p1 = p1;
	   _p2 = p2;
	};

	void operator()() { return (_obj->*_func)(_p1, _p2); }
	void execute() { return operator()(); }

private:

	Class*  _obj;
	Method  _func;
	P1 _p1;
	P2 _p2;
};

template <class Class, typename P1, typename P2, typename P3>
class CallbackP3 : public CallbackBase
{
public:

	typedef void (Class::*Method)(P1, P2, P3);
	CallbackP3(Class* _class_instance, Method _method, P1 p1, P2 p2, P3 p3)
	{
	   _obj = _class_instance;
	   _func = _method;
	   _p1 = p1;
	   _p2 = p2;
	   _p3 = p3;
	};

	void operator()() { return (_obj->*_func)(_p1, _p2, _p3); }
	void execute() { return operator()(); }

private:

	Class*  _obj;
	Method  _func;
	P1 _p1;
	P2 _p2;
	P3 _p3;
};

template <class Class, typename P1, typename P2, typename P3, typename P4>
class CallbackP4 : public CallbackBase
{
public:

	typedef void (Class::*Method)(P1, P2, P3, P4);
	CallbackP4(Class* _class_instance, Method _method, P1 p1, P2 p2, P3 p3, P4 p4)
	{
	   _obj = _class_instance;
	   _func = _method;
	   _p1 = p1;
	   _p2 = p2;
	   _p3 = p3;
	   _p4 = p4;
	};

	void operator()() { return (_obj->*_func)(_p1, _p2, _p3, _p4); }
	void execute() { return operator()(); }

private:

	Class*  _obj;
	Method  _func;
	P1 _p1;
	P2 _p2;
	P3 _p3;
	P4 _p4;
};

#endif

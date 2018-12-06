OpenVPN 3 Linux coding style guide
==================================

The OpenVPN 3 Linux project uses a coding style which aims to be closer
to the OpenVPN 2.x project.  But there are several cases where the coding
style needs to be extended, due to the natural difference between C and
C++.

In addition, the OpenVPN 3 Core Library is loosely based on the GNU style,
which is often quite different.  This coding style is isolated to the
openvpn3-core directory and should not be used in OpenVPN 3 Linux at all.


Basic guideline
---------------

The coding style is mostly based on the Allman/BSD coding style, but with
a few minor tweaks.

- Spaces only.  NO tabs.
- Indenting is 4 spaces.
- Line length less than 78 chars as much as possible.
- Ensure readability, don't be worried adding spaces and blank lines.
- Using the C++11 mandatory.  Do not use newer features if not available
  or functional with compilers only supporting C++11.
- Use C++11 features and constructs over older C++ and C standards.


Naming conventions
------------------
Main goal: Readability.  It should be quick and easy to separate even
quite similar names based on the writing style.

### Classes
Class names is based on camel case where multiple following capital letters
are allowed, like `OpenVPNexampleClass`.  Another example is
`ExampleClass`.

### Class method names
Public methods should always start with a a capital letter.  Otherwise,
similar camel case style can be used, like: `ExampleMethod()`.

Private and protected methods should always be lower case.  To separate
words, use underscore/underbar (`_`), like: `example_method()`

### Function names
Functions are always defined outside a class, and pure functions should
be using lower-case names only.  Static functions which are only available
inside the single source file can carry a separate identified to make it
clear this is a scope local function.

### Variables and variable names
Variable names should normally be lower case only and longer explanatory
names are better.  So instead if `int i` it is preferred `int counter`.

Private members may have an underscore (`_`) after the name, like:
`int private_variable_`

Variables being assigned with a value or being compared should always have
spaces between the name, the operator and the value.

```cpp
   int i = 0;
   bool equal = (4 == i);
```


Member (variable) access
------------------------
Member variables should in classes normally only be accessed via getter
and setter methods.  For struct, where members are public by default,
direct access is fine.


Namespaces
----------
Namespaces may be used, but the `openvpn` namespace is reserved for the
OpenVPN 3 Core library (files under `openvpn3-core/`).  Nesting namespaces
can be done, but no indenting is needed when declaring the namespace chain.
Anything defined inside the namespace must be indented.

The closing bracket of the namespace clearly must declare which namespace
is being closed.

```cpp
namespace NS1
{
namespace NS2
{
namespace NS3
{
    int some_variable; // This is accessed as NS1::NS2::NS3::some_variable
} // namespace NS3
} // namespace NS2
} // namespace NS1
```


Class declarations
------------------
```cpp
/**
 *  Simple explanation in doxygen style clarifying what
 *  this class provides
 */
class ExampleWidget : public ParentClassOne,
                      public ParentClassTwo
{
public:
    /**
     *  (Optional) constructor documentation
     *  explaining arguments and what it constructs
     */
    ExampleWidget(std::string name)
       : ParentClassOne(name),
         ParentClassTwo()
    {
    }


    /**
     *  Doxygen formatted documentation for the
     *  method
     *
     * @param arg  std::string provides an argument....
     *
     */
    void ExampleMethod(std::string arg)
    {
    }


    /**
     *  Similar documentation, this documentation is kept
     *  at the main declaration in the class and not
     *  at the method implementation if this is split up
     *
     * @param arg1  int containing the first argument
     * @param arg2  int containing the second argument
     *
     * @return Explain what this method returns
     */
    int Sum(int arg1, int arg2);


    /**
     *  (similar to above, simplifying here to avoid duplication)
     *  Arguments are aligned as the example below
     */
    bool CompareValues(SomeClass obj_a, SomeClass obj_b,
                       SomeOtherClass obj_other);


private:
    int val1 = 0;  ///< Doxygen comment describing a single member


    void do_some_magic()
    {
         // Magic happens here
    }
}


int ExampleWidget::sum(int arg1, arg2)
{
     // Do something clever with arg1 and arg2
     do_some_magic();
}
```

Classes are separated with minimum 2 blank lines.  Methods as well as
public/protected/private sections are separated with 2 blank lines.


Keywords
--------
C++ keywords, like `if`, `for`, `while`  and related ones will always carry
a space before any brackets.  Curly braces are mandatory for all loop and
conditional statements.

```cpp
   for (const auto& j : some_list)
   {
       std::cout << j << std::endl;
   }

   while (!done)
   {
      // do something
   }

   if (4 == variable)
   {
   }
```

Also note that the left side of a comparison operator should be the one
least likely to be modifiable.  This avoids errors like:

```cpp
    if (i = 3)  // BAD BAD BAD
    {
       // This is incorrect and some compilers might accept it
       // without warning of assignment operation
    }
```


For-loops
---------
For-loops should use C++ iterators as much as possible, wherever it makes
sense.  The variable used to contain the iterated value should be declared
as `const auto&`.


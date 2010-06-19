/*=========================================================================

  Program:   WrapVTK
  Module:    vtkParseProperties.c

  Copyright (c) 2010 David Gobbi
  All rights reserved.

     This software is distributed WITHOUT ANY WARRANTY; without even
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
     PURPOSE.

=========================================================================*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "vtkParse.h"
#include "vtkParseUtils.h"
#include "vtkParseProperties.h"
#include "vtkConfigure.h"

/*-------------------------------------------------------------------
 * A struct that lays out the function information in a way
 * that makes it easy to find methods that act on the same ivars.
 * Only ivar methods will properly fit this struct. */

typedef struct _MethodAttributes
{
  const char *Name;       /* method name */
  int HasProperty;        /* method accesses a property */
  int Type;               /* data type of gettable/settable value */
  int Count;              /* count for gettable/settable value */
  const char *ClassName;  /* class name for if the type is a class */
  const char *Comment;    /* documentation for method */
  int IsPublic;           /* method is public */
  int IsProtected;        /* method is protected */
  int IsLegacy;           /* method is marked "legacy" */
  int IsStatic;           /* method is static */
  int IsRepeat;           /* method is a repeat of a similar method */
  int IsHinted;           /* method has a hint */
  int IsMultiValue;       /* method is e.g. SetValue(x0, x1, x2) */
  int IsIndexed;          /* method is e.g. SetValue(i, val) */
  int IsEnumerated;       /* method is e.g. SetValueToSomething() */
  int IsBoolean;          /* method is ValueOn() or ValueOff() */
} MethodAttributes;

typedef struct _ClassPropertyMethods
{
  int NumberOfMethods;
  MethodAttributes **Methods;
} ClassPropertyMethods;

/*-------------------------------------------------------------------
 * Checks for various common method names for property access */

static int isSetMethod(const char *name)
{
  return (name && name[0] == 'S' && name[1] == 'e' && name[2] == 't' &&
          isupper(name[3]));
}

static int isSetNthMethod(const char *name)
{
  if (isSetMethod(name))
    {
    return (name[3] == 'N' && name[4] == 't' && name[5] == 'h' &&
            isupper(name[6]));
    }

  return 0;
}

static int isSetNumberOfMethod(const char *name)
{
  int n;

  if (isSetMethod(name))
    {
    n = strlen(name);
    return (name[3] == 'N' && name[4] == 'u' && name[5] == 'm' &&
            name[6] == 'b' && name[7] == 'e' && name[8] == 'r' &&
            name[9] == 'O' && name[10] == 'f' && isupper(name[11]) &&
            name[n-1] == 's');
    }

  return 0;
}

static int isGetMethod(const char *name)
{
  return (name && name[0] == 'G' && name[1] == 'e' && name[2] == 't' &&
          isupper(name[3]));
}

static int isGetNthMethod(const char *name)
{
  if (isGetMethod(name))
    {
    return (name[3] == 'N' && name[4] == 't' && name[5] == 'h' &&
            isupper(name[6]));
    }

  return 0;
}

static int isGetNumberOfMethod(const char *name)
{
  int n;

  if (isGetMethod(name))
    {
    n = strlen(name);
    return (name[3] == 'N' && name[4] == 'u' && name[5] == 'm' &&
            name[6] == 'b' && name[7] == 'e' && name[8] == 'r' &&
            name[9] == 'O' && name[10] == 'f' && isupper(name[11]) &&
            name[n-1] == 's');
    }

  return 0;
}

static int isAddMethod(const char *name)
{
  return (name && name[0] == 'A' && name[1] == 'd' && name[2] == 'd' &&
          isupper(name[3]));
}

static int isRemoveMethod(const char *name)
{
  return (name && name[0] == 'R' && name[1] == 'e' && name[2] == 'm' &&
          name[3] == 'o' && name[4] == 'v' && name[5] == 'e' &&
          isupper(name[6]));
}

static int isRemoveAllMethod(const char *name)
{
  int n;

  if (isRemoveMethod(name))
    {
    n = strlen(name);
    return (name[6] == 'A' && name[7] == 'l' && name[8] == 'l' &&
            isupper(name[9]) && name[n-1] == 's');
    }

  return 0;
}

static int isBooleanMethod(const char *name)
{
  int n;

  if (name)
    {
    n = strlen(name);
    if ((n > 2 && name[n-2] == 'O' && name[n-1] == 'n') ||
        (n > 3 && name[n-3] == 'O' && name[n-2] == 'f' && name[n-1] == 'f'))
      {
      return 1;
      }
    }

  return 0;
}

static int isEnumeratedMethod(const char *name)
{
  size_t i, n;

  if (isSetMethod(name))
    {
    n = strlen(name) - 3;
    for (i = 3; i < n; i++)
      {
      if (name[i+0] == 'T' && name[i+1] == 'o' &&
          (isupper(name[i+2]) || isdigit(name[i+2])))
        {
        return 1;
        }
      }
    }

  return 0;
}

static int isAsStringMethod(const char *name)
{
  int n;

  if (isGetMethod(name))
    {
    n = strlen(name);
    if (n > 11)
      {
      if (name[n-8] == 'A' && name[n-7] == 's' && name[n-6] == 'S' &&
          name[n-5] == 't' && name[n-4] == 'r' && name[n-3] == 'i' &&
          name[n-2] == 'n' && name[n-1] == 'g')
        {
        return 1;
        }
      }
    }

  return 0;
}

static int isGetMinValueMethod(const char *name)
{
  int n;

  if (isGetMethod(name))
    {
    n = strlen(name);
    if (n > 11 && strcmp("MinValue", &name[n-8]) == 0)
      {
      return 1;
      }
    }

  return 0;
}

static int isGetMaxValueMethod(const char *name)
{
  int n;

  if (isGetMethod(name))
    {
    n = strlen(name);
    if (n > 11 && strcmp("MaxValue", &name[n-8]) == 0)
      {
      return 1;
      }
    }

  return 0;
}

/*-------------------------------------------------------------------
 * Return the method category bit for the given method, based on the
 * method name and other information in the MethodAttributes struct.
 * If shortForm in on, then suffixes such as On, Off, AsString,
 * and ToSomething are considered while doing the categorization */

static unsigned int methodCategory(MethodAttributes *meth, int shortForm)
{
  int n;
  const char *name;
  name = meth->Name;

  if (isSetMethod(name))
    {
    if (meth->IsEnumerated)
      {
      return VTK_METHOD_ENUM_SET;
      }
    else if (meth->IsIndexed)
      {
      if (isSetNthMethod(name))
        {
        return VTK_METHOD_NTH_SET;
        }
      else
        {
        return VTK_METHOD_INDEX_SET;
        }
      }
    else if (meth->IsMultiValue)
      {
      return VTK_METHOD_MULTI_SET;
      }
    else if (shortForm && isSetNumberOfMethod(name))
      {
      return VTK_METHOD_SET_NUM;
      }
    else
      {
      return VTK_METHOD_BASIC_SET;
      }
    }
  else if (meth->IsBoolean)
    {
    n = strlen(name);
    if (name[n-1] == 'n')
      {
      return VTK_METHOD_BOOL_ON;
      }
    else
      {
      return VTK_METHOD_BOOL_OFF;
      }
    }
  else if (isGetMethod(name))
    {
    if (shortForm && isGetMinValueMethod(name))
      {
      return VTK_METHOD_MIN_GET;
      }
    else if (shortForm && isGetMaxValueMethod(name))
      {
      return VTK_METHOD_MAX_GET;
      }
    else if (shortForm && isAsStringMethod(name))
      {
      return VTK_METHOD_STRING_GET;
      }
    else if (meth->IsIndexed && meth->Count > 0 && !meth->IsHinted)
      {
      if (isGetNthMethod(name))
        {
        return VTK_METHOD_NTH_RHS_GET;
        }
      else
        {
        return VTK_METHOD_INDEX_RHS_GET;
        }
      }
    else if (meth->IsIndexed)
      {
      if (isGetNthMethod(name))
        {
        return VTK_METHOD_NTH_GET;
        }
      else
        {
        return VTK_METHOD_INDEX_GET;
        }
      }
    else if (meth->IsMultiValue)
      {
      return VTK_METHOD_MULTI_GET;
      }
    else if (meth->Count > 0 && !meth->IsHinted)
      {
      return VTK_METHOD_RHS_GET;
      }
    else if (shortForm && isGetNumberOfMethod(name))
      {
      return VTK_METHOD_GET_NUM;
      }
    else
      {
      return VTK_METHOD_BASIC_GET;
      }
    }
  else if (isRemoveMethod(name))
    {
    if (isRemoveAllMethod(name))
      {
      return VTK_METHOD_REMOVEALL;
      }
    else if (meth->IsIndexed)
      {
      return VTK_METHOD_INDEX_REM;
      }
    else
      {
      return VTK_METHOD_BASIC_REM;
      }
    }
  else if (isAddMethod(name))
    {
    if (meth->IsIndexed)
      {
      return VTK_METHOD_INDEX_ADD;
      }
    else if (meth->IsMultiValue)
      {
      return VTK_METHOD_MULTI_ADD;
      }
    else
      {
      return VTK_METHOD_BASIC_ADD;
      }
    }

  return 0;
}

/*-------------------------------------------------------------------
 * remove the following prefixes from a method name:
 * Set, Get, Add, Remove */

static const char *nameWithoutPrefix(const char *name)
{
  if (isGetNthMethod(name) || isSetNthMethod(name))
    {
    return &name[6];
    }
  else if (isGetMethod(name) || isSetMethod(name) || isAddMethod(name))
    {
    return &name[3];
    }
  else if (isRemoveAllMethod(name))
    {
    return &name[9];
    }
  else if (isRemoveMethod(name))
    {
    return &name[6];
    }

  return name;
}

/*-------------------------------------------------------------------
 * check for a valid suffix, i.e. "On" or "Off" or "ToSomething" */

static int isValidSuffix(
  const char *methName, const char *propertyName, const char *suffix)
{
  if ((suffix[0] == 'O' && suffix[1] == 'n' && suffix[2] == '\0') ||
      (suffix[0] == 'O' && suffix[1] == 'f' && suffix[2] == 'f' &&
       suffix[3] == '\0'))
    {
    return 1;
    }

  else if (isSetMethod(methName) &&
      suffix[0] == 'T' && suffix[1] == 'o' &&
      (isupper(suffix[2]) || isdigit(suffix[2])))
    {
    return 1;
    }

  else if (isGetMethod(methName) &&
      ((suffix[0] == 'A' && suffix[1] == 's' &&
       (isupper(suffix[2]) || isdigit(suffix[2]))) ||
      (((suffix[0] == 'M' && suffix[1] == 'a' && suffix[2] == 'x') ||
        (suffix[0] == 'M' && suffix[1] == 'i' && suffix[2] == 'n')) &&
       (suffix[3] == 'V' && suffix[4] == 'a' && suffix[5] == 'l' &&
        suffix[6] == 'u' && suffix[7] == 'e' && suffix[8] == '\0'))))
    {
    return 1;
    }

  else if (isRemoveAllMethod(methName))
    {
    return (suffix[0] == 's' && suffix[1] == '\0');
    }

  else if (isGetNumberOfMethod(methName) ||
           isSetNumberOfMethod(methName))
    {
    if (strncmp(propertyName, "NumberOf", 8) == 0)
      {
      return (suffix[0] == '\0');
      }
    else
      {
      return (suffix[0] == 's' && suffix[1] == '\0');
      }
    }
  else if (suffix[0] == '\0')
    {
    return 1;
    }

  return 0;
}

/*-------------------------------------------------------------------
 * Convert the FunctionInfo into a MethodAttributes, which will make
 * it easier to find matched Set/Get methods.  A return value of zero
 * means the conversion failed, i.e. the method signature is too complex
 * for the MethodAttributes struct */

static int getMethodAttributes(FunctionInfo *func, MethodAttributes *attrs)
{
  size_t i, n;
  int tmptype = 0;
  int allSame = 0;
  int indexed = 0;

  attrs->Name = func->Name;
  attrs->HasProperty = 0; 
  attrs->Type = 0;
  attrs->Count = 0;
  attrs->ClassName = 0;
  attrs->Comment = func->Comment;
  attrs->IsPublic = func->IsPublic;
  attrs->IsProtected = func->IsProtected;
  attrs->IsLegacy = func->IsLegacy;
  attrs->IsStatic = 0;
  attrs->IsRepeat = 0;
  attrs->IsMultiValue = 0;
  attrs->IsHinted = 0;
  attrs->IsIndexed = 0;
  attrs->IsEnumerated = 0;
  attrs->IsBoolean = 0;

  if ((func->ReturnType & VTK_PARSE_STATIC) &&
      func->ReturnType != VTK_PARSE_FUNCTION)
    {
    attrs->IsStatic = 1;
    }

  /* check for major issues with the function */
  if (!func->Name || func->ArrayFailure || func->IsOperator)
    {
    return 0;
    }

  /* check for indexed methods: the first argument will be an integer */
  if (func->NumberOfArguments > 0 &&
      (vtkParse_BaseType(func->ArgTypes[0]) == VTK_PARSE_INT ||
       vtkParse_BaseType(func->ArgTypes[0]) == VTK_PARSE_ID_TYPE) &&
      !vtkParse_TypeIsIndirect(func->ArgTypes[0]))
    {
    /* methods of the form "void SetValue(int i, type value)" */
    if (vtkParse_BaseType(func->ReturnType) == VTK_PARSE_VOID &&
        !vtkParse_TypeIsIndirect(func->ReturnType) &&
        func->NumberOfArguments == 2)
      {
      indexed = 1;

      if (!isSetNumberOfMethod(func->Name))
        {
        /* make sure this isn't a multi-value int method */
        tmptype = func->ArgTypes[0];
        allSame = 1;

        n = func->NumberOfArguments;
        for (i = 0; i < n; i++)
          {
          if (func->ArgTypes[i] != tmptype)
            {
            allSame = 0;
            }
          }
        indexed = !allSame;
        }
      }
    /* methods of the form "type GetValue(int i)" */
    if (!(vtkParse_BaseType(func->ReturnType) == VTK_PARSE_VOID &&
          !vtkParse_TypeIsIndirect(func->ReturnType)) &&
        func->NumberOfArguments == 1)
      {
      indexed = 1;
      }

    attrs->IsIndexed = indexed;
    }

  /* if return type is not void and no args or 1 index */
  if (!(vtkParse_BaseType(func->ReturnType) == VTK_PARSE_VOID &&
        !vtkParse_TypeIsIndirect(func->ReturnType)) &&
      func->NumberOfArguments == indexed)
    {
    /* methods of the form "type GetValue()" or "type GetValue(i)" */
    if (isGetMethod(func->Name))
      {
      attrs->HasProperty = 1;
      attrs->Type = func->ReturnType;
      attrs->Count = (func->HaveHint ? func->HintSize : 0);
      attrs->IsHinted = func->HaveHint;
      attrs->ClassName = func->ReturnClass;

      return 1;
      }
    }

  /* if return type is void and 1 arg or 1 index and 1 arg */
  if (vtkParse_BaseType(func->ReturnType) == VTK_PARSE_VOID &&
      !vtkParse_TypeIsIndirect(func->ReturnType) &&
      func->NumberOfArguments == (1 + indexed))
    {
    /* "void SetValue(type)" or "void SetValue(int, type)" */
    if (isSetMethod(func->Name))
      {
      attrs->HasProperty = 1;
      attrs->Type = func->ArgTypes[indexed];
      attrs->Count = func->ArgCounts[indexed];
      attrs->ClassName = func->ArgClasses[indexed];

      return 1;
      }
    /* "void GetValue(type *)" or "void GetValue(int, type *)" */
    else if (isGetMethod(func->Name) &&
             func->ArgCounts[indexed] > 0 &&
             vtkParse_TypeIsIndirect(func->ArgTypes[indexed]) &&
             !vtkParse_TypeIsConst(func->ArgTypes[indexed]))
      {
      attrs->HasProperty = 1;
      attrs->Type = func->ArgTypes[indexed];
      attrs->Count = func->ArgCounts[indexed];
      attrs->ClassName = func->ArgClasses[indexed];

      return 1;
      }
    /* "void AddValue(vtkObject *)" or "void RemoveValue(vtkObject *)" */
    else if (((isAddMethod(func->Name) || isRemoveMethod(func->Name)) &&
             vtkParse_BaseType(func->ArgTypes[indexed]) == VTK_PARSE_VTK_OBJECT &&
             vtkParse_TypeIndirection(func->ArgTypes[indexed]) == VTK_PARSE_POINTER))
      {
      attrs->HasProperty = 1;
      attrs->Type = func->ArgTypes[indexed];
      attrs->Count = func->ArgCounts[indexed];
      attrs->ClassName = func->ArgClasses[indexed];

      return 1;
      }
    }

  /* check for multiple arguments of the same type */
  if (func->NumberOfArguments > 1 && !indexed)
    {
    tmptype = func->ArgTypes[0];
    allSame = 1;

    n = func->NumberOfArguments;
    for (i = 0; i < n; i++)
      {
      if (func->ArgTypes[i] != tmptype)
        {
        allSame = 0;
        }
      }

    if (allSame)
      {
      /* "void SetValue(type x, type y, type z)" */
      if (isSetMethod(func->Name) && !vtkParse_TypeIsIndirect(tmptype) &&
          vtkParse_BaseType(func->ReturnType) == VTK_PARSE_VOID &&
          !vtkParse_TypeIsIndirect(func->ReturnType))
        {
        attrs->HasProperty = 1;
        attrs->Type = tmptype;
        attrs->Count = n;
        attrs->IsMultiValue = 1;

        return 1;
        }
      /* "void GetValue(type& x, type& x, type& x)" */
      else if (isGetMethod(func->Name) &&
               vtkParse_TypeIndirection(tmptype) == VTK_PARSE_REF &&
               !vtkParse_TypeIsConst(tmptype) &&
               vtkParse_BaseType(func->ReturnType) == VTK_PARSE_VOID &&
              !vtkParse_TypeIsIndirect(func->ReturnType))
        {
        attrs->HasProperty = 1;
        attrs->Type = tmptype;
        attrs->Count = n;
        attrs->IsMultiValue = 1;

        return 1;
        }
      /* "void AddValue(type x, type y, type z)" */
      else if (isAddMethod(func->Name) && !vtkParse_TypeIsIndirect(tmptype) &&
               (vtkParse_BaseType(func->ReturnType) == VTK_PARSE_VOID ||
                vtkParse_BaseType(func->ReturnType) == VTK_PARSE_INT ||
                vtkParse_BaseType(func->ReturnType) == VTK_PARSE_ID_TYPE) &&
               !vtkParse_TypeIsIndirect(func->ReturnType))
        {
        attrs->HasProperty = 1;
        attrs->Type = tmptype;
        attrs->Count = n;
        attrs->IsMultiValue = 1;

        return 1;
        }
      }
    }

  /* if return type is void, and there are no arguments */
  if (vtkParse_BaseType(func->ReturnType) == VTK_PARSE_VOID &&
      !vtkParse_TypeIsIndirect(func->ReturnType) &&
      func->NumberOfArguments == 0)
    {
    attrs->Type = VTK_PARSE_VOID;

    /* "void ValueOn()" or "void ValueOff()" */
    if (isBooleanMethod(func->Name))
      {
      attrs->HasProperty = 1;
      attrs->IsBoolean = 1;
      return 1;
      }
    /* "void SetValueToEnum()" */
    else if (isEnumeratedMethod(func->Name))
      {
      attrs->HasProperty = 1;
      attrs->IsEnumerated = 1;
      return 1;
      }
    /* "void RemoveAllValues()" */
    else if (isRemoveAllMethod(func->Name))
      {
      attrs->HasProperty = 1;
      return 1;
      }
    }

  return 0;
}

/*-------------------------------------------------------------------
 * Check to see if the specified method is a match with the specified
 * property, i.e the name, type, and array count of the property
 * must match.  The longMatch value is set to '1' if the prefix/suffix
 * was part of the name match. */

static int methodMatchesProperty(
  PropertyInfo *property, MethodAttributes *meth, int *longMatch)
{
  size_t n;
  int propertyType, methType;
  const char *propertyName;
  const char *name;
  const char *methSuffix;
  int methodBitfield = 0;

  /* get the bitfield containing all found methods for this property */
  if (meth->IsPublic)
    {
    methodBitfield = property->PublicMethods;
    }
  else if (meth->IsProtected)
    {
    methodBitfield = property->ProtectedMethods;
    }
  else
    {
    methodBitfield = property->PrivateMethods;
    }

  /* get the property name and compare it to the method name */
  propertyName = property->Name;
  name = nameWithoutPrefix(meth->Name);

  if (name == 0 || propertyName == 0)
    {
    return 0;
    }

  /* longMatch is only set for full matches of GetNumberOf(),
   * SetNumberOf(), GetVarMinValue(), GetVarMaxValue() methods */
  *longMatch = 0;
  n = strlen(propertyName);
  if (isGetNumberOfMethod(meth->Name) || isSetNumberOfMethod(meth->Name))
    {
    if (strncmp(propertyName, "NumberOf", 8) == 0 && isupper(propertyName[8]))
      {
      /* longer match */
      *longMatch = 1;
      }
    else
      {
      /* longer prefix */
      name = &meth->Name[11];
      }
    }
  else if (isGetMinValueMethod(meth->Name))
    {
    if (n >= 8 && strcmp(&propertyName[n-8], "MinValue") == 0)
      {
      *longMatch = 1;
      }
    }
  else if (isGetMaxValueMethod(meth->Name))
    {
    if (n >= 8 && strcmp(&propertyName[n-8], "MaxValue") == 0)
      {
      *longMatch = 1;
      }
    }
  else if (isAsStringMethod(meth->Name))
    {
    if (n >= 8 && strcmp(&propertyName[n-8], "AsString") == 0)
      {
      *longMatch = 1;
      }
    }

  /* make sure the method name contains the property name */
  if (!strncmp(name, propertyName, n) == 0)
    {
    return 0;
    }

  /* make sure that any non-matching bits are valid suffixes */
  methSuffix = &name[n];
  if (!isValidSuffix(meth->Name, propertyName, methSuffix))
    {
    return 0;
    }

  /* check for type match */
  methType = meth->Type;
  propertyType = property->Type;

  /* remove "const" and "static" */
  if (vtkParse_TypeHasQualifier(methType))
    {
    methType = (methType & VTK_PARSE_UNQUALIFIED_TYPE);
    }

  /* check for RemoveAll method matching an Add method*/
  if (isRemoveAllMethod(meth->Name) &&
      methType == VTK_PARSE_VOID &&
      !vtkParse_TypeIsIndirect(methType) &&
      ((methodBitfield & (VTK_METHOD_BASIC_ADD | VTK_METHOD_MULTI_ADD)) != 0))
    {
    return 1;
    }

  /* check for GetNumberOf and SetNumberOf for indexed properties */
  if (isGetNumberOfMethod(meth->Name) &&
      (methType == VTK_PARSE_INT || methType == VTK_PARSE_ID_TYPE) &&
      !vtkParse_TypeIsIndirect(methType) &&
      ((methodBitfield & (VTK_METHOD_INDEX_GET | VTK_METHOD_NTH_GET)) != 0))
    {
    return 1;
    }

  if (isSetNumberOfMethod(meth->Name) &&
      (methType == VTK_PARSE_INT || methType == VTK_PARSE_ID_TYPE) &&
      !vtkParse_TypeIsIndirect(methType) &&
      ((methodBitfield & (VTK_METHOD_INDEX_SET | VTK_METHOD_NTH_SET)) != 0))
    {
    return 1;
    }

  /* remove ampersands i.e. "ref" */
  if (vtkParse_TypeIndirection(methType) == VTK_PARSE_REF)
    {
    methType = (methType & ~VTK_PARSE_INDIRECT);
    }
  else if (vtkParse_TypeIndirection(methType) == VTK_PARSE_POINTER_REF)
    {
    methType = ((methType & ~VTK_PARSE_INDIRECT) | VTK_PARSE_POINTER);
    }
  else if (vtkParse_TypeIndirection(methType) == VTK_PARSE_CONST_POINTER_REF)
    {
    methType = ((methType & ~VTK_PARSE_INDIRECT) | VTK_PARSE_CONST_POINTER);
    }

  /* if method is multivalue, e.g. SetColor(r,g,b), then the
   * referenced property is a pointer */
  if (meth->IsMultiValue)
    {
    if (vtkParse_TypeIndirection(methType) == VTK_PARSE_POINTER)
      {
      methType = (methType & ~VTK_PARSE_INDIRECT);
      methType = (methType | VTK_PARSE_POINTER_POINTER);
      }
    else if (vtkParse_TypeIndirection(methType) == 0)
      {
      methType = (methType | VTK_PARSE_POINTER);
      }
    else
      {
      return 0;
      }
    }

  /* promote "void" to enumerated type for e.g. boolean methods, and */
  /* check for GetValueAsString method, assume it has matching enum */
  if (meth->IsBoolean || meth->IsEnumerated ||
      (isAsStringMethod(meth->Name) &&
       vtkParse_BaseType(methType) == VTK_PARSE_CHAR &&
       vtkParse_TypeIndirection(methType) == VTK_PARSE_POINTER))
    {
    if (!vtkParse_TypeIsIndirect(propertyType) &&
        (propertyType == VTK_PARSE_INT ||
         propertyType == VTK_PARSE_UNSIGNED_INT ||
         propertyType == VTK_PARSE_UNSIGNED_CHAR ||
         (meth->IsBoolean && propertyType == VTK_PARSE_BOOL)))
      {
      methType = propertyType;
      }
    }

  /* check for matched type and count */
  if (methType != propertyType || meth->Count != property->Count)
    {
    return 0;
    }

  /* if vtkObject, check that classes match */
  if (vtkParse_BaseType(methType) == VTK_PARSE_VTK_OBJECT)
    {
    if (meth->IsMultiValue || !vtkParse_TypeIsPointer(methType) ||
        meth->Count != 0 ||
        meth->ClassName == 0 || property->ClassName == 0 ||
        strcmp(meth->ClassName, property->ClassName) != 0)
      {
      return 0;
      }
    }

  /* done, match was found! */
  return 1;
}

/*-------------------------------------------------------------------
 * initialize a PropertyInfo struct from a MethodAttributes
 * struct, only valid if the method name has no suffixes such as
 * On/Off, AsString, ToSomething, RemoveAllSomethings, etc. */

static void initializePropertyInfo(
  PropertyInfo *property, MethodAttributes *meth, unsigned int methodBit)
{
  int type;
  type = meth->Type;

  /* for ValueOn()/Off() or SetValueToEnum() methods, set type to int */
  if (meth->IsBoolean || meth->IsEnumerated)
    {
    type = VTK_PARSE_INT;
    }

  property->Name = nameWithoutPrefix(meth->Name);

  /* get property type, but don't include "ref" as part of type,
   * and use a pointer if the method is multi-valued */
  property->Type = vtkParse_BaseType(type);
  if ((!meth->IsMultiValue &&
       (vtkParse_TypeIndirection(type) == VTK_PARSE_POINTER ||
        vtkParse_TypeIndirection(type) == VTK_PARSE_POINTER_REF)) ||
      (meth->IsMultiValue &&
       (vtkParse_TypeIndirection(type) == 0 ||
        vtkParse_TypeIndirection(type) == VTK_PARSE_REF)))
    {
    property->Type = (property->Type | VTK_PARSE_POINTER);
    }
  else if ((!meth->IsMultiValue &&
       (vtkParse_TypeIndirection(type) == VTK_PARSE_CONST_POINTER ||
        vtkParse_TypeIndirection(type) == VTK_PARSE_CONST_POINTER_REF)))
    {
    property->Type = (property->Type | VTK_PARSE_CONST_POINTER);
    }
  else if (vtkParse_TypeIndirection(type) == VTK_PARSE_POINTER_POINTER ||
           (vtkParse_TypeIndirection(type) == VTK_PARSE_POINTER &&
            meth->IsMultiValue))
    {
    property->Type = (property->Type | VTK_PARSE_POINTER_POINTER);
    }

  property->ClassName = meth->ClassName;
  property->Count = meth->Count;
  property->IsStatic = meth->IsStatic;
  property->EnumConstantNames = 0;
  property->PublicMethods = 0;
  property->ProtectedMethods = 0;
  property->PrivateMethods = 0;
  property->LegacyMethods = 0;
  property->Comment = meth->Comment;

  if (meth->IsPublic)
    {
    property->PublicMethods = methodBit;
    }
  else if (meth->IsProtected)
    {
    property->ProtectedMethods = methodBit;
    }
  else
    {
    property->PrivateMethods = methodBit;
    }

  if (meth->IsLegacy)
    {
    property->LegacyMethods = methodBit;
    }
}

/*-------------------------------------------------------------------
 * Find all the methods that match the specified method, and add
 * flags to the PropertyInfo struct */

static void findAllMatches(
  PropertyInfo *property, int propertyId, ClassPropertyMethods *methods,
  int matchedMethods[], unsigned int methodCategories[],
  int methodProperties[])
{
  int i, j, k, n;
  size_t m;
  MethodAttributes *meth;
  unsigned int methodBit;
  int longMatch;
  int foundNoMatches = 0;

  n = methods->NumberOfMethods;

  /* loop repeatedly until no more matches are found */
  while (!foundNoMatches)
    {
    foundNoMatches = 1;

    for (i = 0; i < n; i++)
      {
      if (matchedMethods[i]) { continue; }

      meth = methods->Methods[i];
      if (methodMatchesProperty(property, meth, &longMatch))
        {
        matchedMethods[i] = 1;
        foundNoMatches = 0;

        /* if any method is static, the property is static */
        if (meth->IsStatic)
          {
          property->IsStatic = 1;
          }

        /* add this as a member of the method bitfield, and consider method
         * suffixes like On, MaxValue, etc. while doing the categorization */
        methodBit = methodCategory(meth, !longMatch);
        methodCategories[i] = methodBit;
        methodProperties[i] = propertyId;

        if (meth->IsPublic)
          {
          property->PublicMethods |= methodBit;
          }
        else if (meth->IsProtected)
          {
          property->ProtectedMethods |= methodBit;
          }
        else
          {
          property->PrivateMethods |= methodBit;
          }

        if (meth->IsLegacy)
          {
          property->LegacyMethods |= methodBit;
          }

        if (meth->IsEnumerated)
          {
          m = strlen(property->Name);
          if (meth->Name[3+m] == 'T' && meth->Name[4+m] == 'o' &&
              (isdigit(meth->Name[5+m]) || isupper(meth->Name[5+m])))
            {
            if (property->EnumConstantNames == 0)
              {
              property->EnumConstantNames =
                (const char **)malloc(sizeof(char *)*8);
              property->EnumConstantNames[0] = 0;
              }

            j = 0;
            while (property->EnumConstantNames[j] != 0) { j++; }
            property->EnumConstantNames[j++] = &meth->Name[5+m];
            if (j % 8 == 0)
              {
              const char **savenames = property->EnumConstantNames; 
              property->EnumConstantNames =
                (const char **)malloc(sizeof(char *)*(j+8));
              for (k = 0; k < j; k++)
                {
                property->EnumConstantNames[k] = savenames[k];
                }
              free((void *)savenames);
              }
            property->EnumConstantNames[j] = 0;
            }
          }
        }
      }
    }
}

/*-------------------------------------------------------------------
 * search for methods that are repeated with minor variations */
static int searchForRepeatedMethods(
  ClassProperties *properties, ClassPropertyMethods *methods, int j)
{
  int i, n;
  MethodAttributes *attrs;
  MethodAttributes *meth;
  n = methods->NumberOfMethods;

  attrs = methods->Methods[j];

  for (i = 0; i < n; i++)
    {
    meth = methods->Methods[i];

    /* check whether the function name and basic structure are matched */
    if (meth->Name && strcmp(attrs->Name, meth->Name) == 0 &&
        (vtkParse_TypeIndirection(attrs->Type) ==
          vtkParse_TypeIndirection(meth->Type)) &&
        attrs->IsPublic == meth->IsPublic &&
        attrs->IsProtected == meth->IsProtected &&
        attrs->IsHinted == meth->IsHinted &&
        attrs->IsMultiValue == meth->IsMultiValue &&
        attrs->IsIndexed == meth->IsIndexed &&
        attrs->IsEnumerated == meth->IsEnumerated &&
        attrs->IsBoolean == meth->IsBoolean)
      {
      /* check to see if the types are compatible:
       * prefer "double" over "float",
       * prefer higher-counted arrays,
       * prefer non-legacy methods */

      if ((vtkParse_BaseType(attrs->Type) == VTK_PARSE_FLOAT &&
           vtkParse_BaseType(meth->Type) == VTK_PARSE_DOUBLE) ||
          (vtkParse_BaseType(attrs->Type) == vtkParse_BaseType(meth->Type) &&
           attrs->Count < meth->Count) ||
          (attrs->IsLegacy && !meth->IsLegacy))
        {
        /* keep existing method */
        attrs->IsRepeat = 1;
        if (properties)
          {
          properties->MethodTypes[j] = properties->MethodTypes[i];
          properties->MethodProperties[j] = properties->MethodProperties[i];
          }
        return 0;
        }

      if ((vtkParse_BaseType(attrs->Type) == VTK_PARSE_DOUBLE &&
           vtkParse_BaseType(meth->Type) == VTK_PARSE_FLOAT) ||
          (vtkParse_BaseType(attrs->Type) == vtkParse_BaseType(meth->Type) &&
           attrs->Count > meth->Count) ||
          (!attrs->IsLegacy && meth->IsLegacy))
        {
        /* keep this method */
        meth->IsRepeat = 1;
        if (properties)
          {
          properties->MethodTypes[i] = properties->MethodTypes[j];
          properties->MethodProperties[i] = properties->MethodProperties[j];
          }
        return 0;
        }
      }
    }

  /* no matches */
  return 1;
}

/*-------------------------------------------------------------------
 * Add a property, using method at index i as a template */

static void addProperty(
  ClassProperties *properties, ClassPropertyMethods *methods, int i,
  int matchedMethods[])
{ 
  MethodAttributes *meth = methods->Methods[i];
  PropertyInfo *property;
  unsigned int category;

  /* save the info about the method used to discover the property */
  matchedMethods[i] = 1;
  category = methodCategory(meth, 0);
  properties->MethodTypes[i] = category;
  properties->MethodProperties[i] = properties->NumberOfProperties;
  /* duplicate the info for all "repeat" methods */
  searchForRepeatedMethods(properties, methods, i);

  /* create the property */
  property = (PropertyInfo *)malloc(sizeof(PropertyInfo));
  initializePropertyInfo(property, meth, category);
  findAllMatches(property, properties->NumberOfProperties, methods,
                 matchedMethods, properties->MethodTypes,
                 properties->MethodProperties);

  properties->Properties[properties->NumberOfProperties++] = property;
}

/*-------------------------------------------------------------------
 * This is the method that finds out everything that it can about
 * all properties that can be accessed by the methods of a class */

static void categorizeProperties(
  ClassPropertyMethods *methods, ClassProperties *properties)
{
  int i, n;
  int *matchedMethods;

  properties->NumberOfProperties = 0;

  n = methods->NumberOfMethods;
  matchedMethods = (int *)malloc(sizeof(int)*n);
  for (i = 0; i < n; i++)
    {
    /* "matchedMethods" are methods removed from consideration */
    matchedMethods[i] = 0;
    if (!methods->Methods[i]->HasProperty || methods->Methods[i]->IsRepeat)
      {
      matchedMethods[i] = 1;
      }
    }

  /* start with the set methods */
  for (i = 0; i < n; i++)
    {
    /* all set methods except for SetValueToEnum() methods
     * and SetNumberOf() methods */
    if (!matchedMethods[i] && isSetMethod(methods->Methods[i]->Name) &&
        !methods->Methods[i]->IsEnumerated &&
        !isSetNumberOfMethod(methods->Methods[i]->Name))
      {
      addProperty(properties, methods, i, matchedMethods);
      }
    }

  /* sweep SetNumberOf() methods that didn't have
   * matching indexed Set methods */
  for (i = 0; i < n; i++)
    {
    if (!matchedMethods[i] && isSetNumberOfMethod(methods->Methods[i]->Name))
      {
      addProperty(properties, methods, i, matchedMethods);
      }
    }

  /* next do the get methods that didn't have matching set methods */
  for (i = 0; i < n; i++)
    {
    /* all get methods except for GetValueAs() methods
     * and GetNumberOf() methods */
    if (!matchedMethods[i] && isGetMethod(methods->Methods[i]->Name) &&
        !isAsStringMethod(methods->Methods[i]->Name) &&
        !isGetNumberOfMethod(methods->Methods[i]->Name))
      {
      addProperty(properties, methods, i, matchedMethods);
      }
    }

  /* sweep the GetNumberOf() methods that didn't have
   * matching indexed Get methods */
  for (i = 0; i < n; i++)
    {
    if (!matchedMethods[i] && isGetNumberOfMethod(methods->Methods[i]->Name))
      {
      addProperty(properties, methods, i, matchedMethods);
      }
    }

  /* finally do the add methods */
  for (i = 0; i < n; i++)
    {
    /* all add methods */
    if (!matchedMethods[i] && isAddMethod(methods->Methods[i]->Name))
      {
      addProperty(properties, methods, i, matchedMethods);
      }
    }

  free(matchedMethods);
}

/*-------------------------------------------------------------------
 * categorize methods that get/set/add/remove values */

static void categorizePropertyMethods(
  ClassInfo *data, ClassPropertyMethods *methods)
{
  int i, n;
  FunctionInfo *func;
  MethodAttributes *attrs;

  methods->NumberOfMethods = 0;

  /* build up the ClassPropertyMethods struct */
  n = data->NumberOfFunctions;
  for (i = 0; i < n; i++)
    {
    func = data->Functions[i];
    attrs = (MethodAttributes *)malloc(sizeof(MethodAttributes));
    methods->Methods[methods->NumberOfMethods++] = attrs;

    /* copy the func into a MethodAttributes struct if possible */
    if (getMethodAttributes(func, attrs))
      {
      /* check for repeats e.g. SetPoint(float *), SetPoint(double *) */
      searchForRepeatedMethods(0, methods, i);
      }
    }
}

/*-------------------------------------------------------------------
 * build a ClassProperties struct from the info in a FileInfo struct */

ClassProperties *vtkParseProperties_Create(ClassInfo *data)
{
  int i;
  ClassProperties *properties;
  ClassPropertyMethods *methods;

  methods = (ClassPropertyMethods *)malloc(sizeof(ClassPropertyMethods));
  methods->Methods = (MethodAttributes **)malloc(sizeof(MethodAttributes *)*
                                                 data->NumberOfFunctions);

  /* categorize the methods according to what properties they reference
   * and what they do to that property */
  categorizePropertyMethods(data, methods);

  properties = (ClassProperties *)malloc(sizeof(ClassProperties));
  properties->NumberOfProperties = 0;
  properties->NumberOfMethods = methods->NumberOfMethods;
  properties->Properties =
    (PropertyInfo **)malloc(sizeof(PropertyInfo *)*methods->NumberOfMethods);
  properties->MethodTypes =
    (unsigned int *)malloc(sizeof(unsigned int)*methods->NumberOfMethods);
  properties->MethodProperties =
    (int *)malloc(sizeof(int)*methods->NumberOfMethods);

  for (i = 0; i < methods->NumberOfMethods; i++)
    {
    properties->MethodTypes[i] = 0;
    properties->MethodProperties[i] = -1;
    }

  /* synthesize a list of properties from the list of methods */
  categorizeProperties(methods, properties);

  for (i = 0; i < methods->NumberOfMethods; i++)
    {
    free(methods->Methods[i]);
    }

  free(methods->Methods);
  free(methods);

  return properties;
}

/*-------------------------------------------------------------------
 * free a class properties struct */

void vtkParseProperties_Free(ClassProperties *properties)
{
  int i, n;

  n = properties->NumberOfProperties;
  for (i = 0; i < n; i++)
    {
    free(properties->Properties[i]);
    }

  free(properties->Properties);
  free(properties->MethodTypes);
  free(properties->MethodProperties);
  free(properties);
}

/*-------------------------------------------------------------------
 * get a string representation of method bitfield value */

const char *vtkParseProperties_MethodTypeAsString(unsigned int methodType)
{
  switch (methodType)
    {
    case VTK_METHOD_BASIC_GET:
      return "BASIC_GET";
    case VTK_METHOD_BASIC_SET:
      return "BASIC_SET";
    case VTK_METHOD_MULTI_GET:
      return "MULTI_GET";
    case VTK_METHOD_MULTI_SET:
      return "MULTI_SET";
    case VTK_METHOD_INDEX_GET:
      return "INDEX_GET";
    case VTK_METHOD_INDEX_SET:
      return "INDEX_SET";
    case VTK_METHOD_NTH_GET:
      return "NTH_GET";
    case VTK_METHOD_NTH_SET:
      return "NTH_SET";
    case VTK_METHOD_RHS_GET:
      return "RHS_GET";
    case VTK_METHOD_INDEX_RHS_GET:
      return "INDEX_RHS_GET";
    case VTK_METHOD_NTH_RHS_GET:
      return "NTH_RHS_GET";
    case VTK_METHOD_STRING_GET:
      return "STRING_GET";
    case VTK_METHOD_ENUM_SET:
      return "ENUM_SET";
    case VTK_METHOD_BOOL_ON:
      return "BOOL_ON";
    case VTK_METHOD_BOOL_OFF:
      return "BOOL_OFF";
    case VTK_METHOD_MIN_GET:
      return "MIN_GET";
    case VTK_METHOD_MAX_GET:
      return "MAX_GET";
    case VTK_METHOD_GET_NUM:
      return "GET_NUM";
    case VTK_METHOD_SET_NUM:
      return "SET_NUM";
    case VTK_METHOD_BASIC_ADD:
      return "BASIC_ADD";
    case VTK_METHOD_MULTI_ADD:
      return "MULTI_ADD";
    case VTK_METHOD_INDEX_ADD:
      return "INDEX_ADD";
    case VTK_METHOD_BASIC_REM:
      return "BASIC_REM";
    case VTK_METHOD_INDEX_REM:
      return "INDEX_REM";
    case VTK_METHOD_REMOVEALL:
      return "REMOVEALL";
    }

  return "";
}
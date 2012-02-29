/*=========================================================================

  Program:   Visualization Toolkit
  Module:    vtkParseData.h

  Copyright (c) Ken Martin, Will Schroeder, Bill Lorensen
  All rights reserved.
  See Copyright.txt or http://www.kitware.com/Copyright.htm for details.

     This software is distributed WITHOUT ANY WARRANTY; without even
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
     PURPOSE.  See the above copyright notice for more information.

=========================================================================*/

/*
  Data structures used by vtkParse.
*/

#ifndef VTK_PARSE_DATA_H
#define VTK_PARSE_DATA_H

#include "vtkParseType.h"

/* legacy */
#define MAX_ARGS 20

/**
 * Access flags
 */
typedef enum _parse_access_t
{
  VTK_ACCESS_PUBLIC    = 0,
  VTK_ACCESS_PROTECTED = 1,
  VTK_ACCESS_PRIVATE   = 2
} parse_access_t;

/**
 * ItemType constants
 */
typedef enum _parse_item_t
{
  VTK_NAMESPACE_INFO = 1,
  VTK_CLASS_INFO     = 2,
  VTK_STRUCT_INFO    = 3,
  VTK_UNION_INFO     = 4,
  VTK_ENUM_INFO      = 5,
  VTK_FUNCTION_INFO  = 6,
  VTK_VARIABLE_INFO  = 7,
  VTK_CONSTANT_INFO  = 8,
  VTK_TYPEDEF_INFO   = 9,
  VTK_USING_INFO     = 10
} parse_item_t;

/**
 * ItemInfo just contains an index
 */
typedef struct _ItemInfo
{
  parse_item_t   Type;
  unsigned long  Index;
} ItemInfo;

/* forward declarations */
struct _TemplateArgs;
struct _FunctionInfo;

/**
 * TemplateArg holds one template arg
 */
typedef struct _TemplateArg
{
  unsigned int  Type;  /* is zero for "typename", "class", "template" */
  const char   *Class; /* class name for type */
  const char   *Name;  /* name of template arg */
  const char   *Value; /* default value */
  unsigned long NumberOfDimensions; /* dimensionality for arrays */
  const char  **Dimensions; /* dimensions for arrays */
  struct _FunctionInfo *Function;  /* for function pointer values */
  struct _TemplateArgs *Template; /* for templated template args */
} TemplateArg;

/**
 * TemplateArgs holds template definitions
 */
typedef struct _TemplateArgs
{
  unsigned long NumberOfArguments;
  TemplateArg **Arguments;
} TemplateArgs;

/**
 * ValueInfo is for typedefs, constants, variables,
 * function arguments, and return values
 *
 * Note that Dimensions is an array of char pointers, in
 * order to support dimensions that are sized according to
 * template arg values or accoring to named constants.
 */
typedef struct _ValueInfo
{
  parse_item_t   ItemType;
  parse_access_t Access;
  const char    *Name;
  const char    *Comment;
  const char    *Value;      /* for vars or default arg values */
  unsigned int   Type;       /* as defined in vtkParseType.h   */
  const char    *Class;      /* classname for type */
  unsigned long  Count;      /* total number of values, if known */
  const char    *CountHint;  /* hint about how to get the count */
  unsigned long  NumberOfDimensions; /* dimensionality for arrays */
  const char   **Dimensions; /* dimensions for arrays */
  struct _FunctionInfo *Function;  /* for function pointer values */
  int            IsStatic;   /* for class variables only */
  int            IsEnum;     /* for constants only */
} ValueInfo;

/**
 * FunctionInfo is for functions and methods
 */
typedef struct _FunctionInfo
{
  parse_item_t   ItemType;
  parse_access_t Access;
  const char    *Name;
  const char    *Comment;
  const char    *Class;       /* class name for methods */
  const char    *Signature;   /* function signature as text */
  TemplateArgs  *Template;    /* template args, or NULL */
  unsigned long  NumberOfArguments;
  ValueInfo    **Arguments;
  ValueInfo     *ReturnValue; /* NULL for constructors and destructors */
  const char    *Macro;       /* the macro that defined this function */
  const char    *SizeHint;    /* hint the size e.g. for operator[] */
  int            IsOperator;
  int            IsVariadic;
  int            IsLegacy;    /* marked as a legacy method or function */
  int            IsStatic;    /* methods only */
  int            IsVirtual;   /* methods only */
  int            IsPureVirtual; /* methods only */
  int            IsConst;     /* methods only */
  int            IsExplicit;  /* constructors only */
  unsigned int   ArgTypes[MAX_ARGS];  /* legacy */
  const char    *ArgClasses[MAX_ARGS];/* legacy */
  unsigned long  ArgCounts[MAX_ARGS]; /* legacy */
  unsigned int   ReturnType;  /* legacy */
  const char    *ReturnClass; /* legacy */
  int            HaveHint;    /* legacy */
  unsigned long  HintSize;    /* legacy */
  int            ArrayFailure;/* legacy */
  int            IsPublic;    /* legacy */
  int            IsProtected; /* legacy */
} FunctionInfo;

/**
 * EnumInfo is for enums
 * Constants are at the same level as the Enum, not inside it.
 */
typedef struct _EnumInfo
{
  parse_item_t   ItemType;
  parse_access_t Access;
  const char    *Name;
  const char    *Comment;
} EnumInfo;

/**
 * UsingInfo is for using directives
 */
typedef struct _UsingInfo
{
  parse_item_t   ItemType;
  parse_access_t Access;
  const char    *Name;     /* null for using whole namespace */
  const char    *Comment;
  const char    *Scope;    /* the namespace or class */
} UsingInfo;

/**
 * ClassInfo is for classes, structs, and unions
 */
typedef struct _ClassInfo
{
  parse_item_t   ItemType;
  parse_access_t Access;
  const char    *Name;
  const char    *Comment;
  TemplateArgs  *Template;
  unsigned long  NumberOfSuperClasses;
  const char   **SuperClasses;
  unsigned long  NumberOfItems;
  ItemInfo      *Items;
  unsigned long  NumberOfClasses;
  struct _ClassInfo **Classes;
  unsigned long  NumberOfFunctions;
  FunctionInfo **Functions;
  unsigned long  NumberOfConstants;
  ValueInfo    **Constants;
  unsigned long  NumberOfVariables;
  ValueInfo    **Variables;
  unsigned long  NumberOfEnums;
  EnumInfo     **Enums;
  unsigned long  NumberOfTypedefs;
  ValueInfo    **Typedefs;
  unsigned long  NumberOfUsings;
  UsingInfo    **Usings;
  int            IsAbstract;
  int            HasDelete;
} ClassInfo;

/**
 * Namespace is for namespaces
 */
typedef struct _NamespaceInfo
{
  parse_item_t   ItemType;
  parse_access_t Access;
  const char    *Name;  /* NULL for global namespace */
  const char    *Comment;
  unsigned long  NumberOfItems;
  ItemInfo      *Items;
  unsigned long  NumberOfClasses;
  ClassInfo    **Classes;
  unsigned long  NumberOfFunctions;
  FunctionInfo **Functions;
  unsigned long  NumberOfConstants;
  ValueInfo    **Constants;
  unsigned long  NumberOfVariables;
  ValueInfo    **Variables;
  unsigned long  NumberOfEnums;
  EnumInfo     **Enums;
  unsigned long  NumberOfTypedefs;
  ValueInfo    **Typedefs;
  unsigned long  NumberOfUsings;
  UsingInfo    **Usings;
  unsigned long  NumberOfNamespaces;
  struct _NamespaceInfo **Namespaces;
} NamespaceInfo;

/**
 * FileInfo is for header files
 */
typedef struct _FileInfo
{
  const char *FileName;
  const char *NameComment;
  const char *Description;
  const char *Caveats;
  const char *SeeAlso;

  ClassInfo *MainClass;
  NamespaceInfo *Contents;
} FileInfo;


#ifdef __cplusplus
extern "C" {
#endif

/**
 * Initializer methods
 */
/*@{*/
void vtkParse_InitFile(FileInfo *file_info);
void vtkParse_InitNamespace(NamespaceInfo *namespace_info);
void vtkParse_InitClass(ClassInfo *cls);
void vtkParse_InitFunction(FunctionInfo *func);
void vtkParse_InitValue(ValueInfo *val);
void vtkParse_InitEnum(EnumInfo *item);
void vtkParse_InitUsing(UsingInfo *item);
void vtkParse_InitTemplateArgs(TemplateArgs *arg);
void vtkParse_InitTemplateArg(TemplateArg *arg);
/*@}*/

/**
 * Copy methods
 *
 * Strings are not deep-copied, they are assumed to be persistent.
 */
/*@{*/
void vtkParse_CopyNamespace(NamespaceInfo *data, const NamespaceInfo *orig);
void vtkParse_CopyClass(ClassInfo *data, const ClassInfo *orig);
void vtkParse_CopyFunction(FunctionInfo *data, const FunctionInfo *orig);
void vtkParse_CopyValue(ValueInfo *data, const ValueInfo *orig);
void vtkParse_CopyEnum(EnumInfo *data, const EnumInfo *orig);
void vtkParse_CopyUsing(UsingInfo *data, const UsingInfo *orig);
void vtkParse_CopyTemplateArgs(TemplateArgs *data, const TemplateArgs *orig);
void vtkParse_CopyTemplateArg(TemplateArg *data, const TemplateArg *orig);
/*@}*/

/**
 * Free methods
 *
 * Strings are not freed, they are assumed to be persistent.
 */
/*@{*/
void vtkParse_FreeFile(FileInfo *file_info);
void vtkParse_FreeNamespace(NamespaceInfo *namespace_info);
void vtkParse_FreeClass(ClassInfo *cls);
void vtkParse_FreeFunction(FunctionInfo *func);
void vtkParse_FreeValue(ValueInfo *val);
void vtkParse_FreeEnum(EnumInfo *item);
void vtkParse_FreeUsing(UsingInfo *item);
void vtkParse_FreeTemplateArgs(TemplateArgs *arg);
void vtkParse_FreeTemplateArg(TemplateArg *arg);
/*@}*/


/**
 * Add a string to an array of strings, grow array as necessary.
 */
void vtkParse_AddStringToArray(
  const char ***valueArray, unsigned long *count, const char *value);

/**
 * Expand the Item array for classes and namespaces.
 */
void vtkParse_AddItemToArray(
  ItemInfo **valueArray, unsigned long *count,
  parse_item_t type, unsigned long idx);


/**
 * Add various items to the structs.
 */
/*@{*/
void vtkParse_AddClassToClass(ClassInfo *info, ClassInfo *item);
void vtkParse_AddFunctionToClass(ClassInfo *info, FunctionInfo *item);
void vtkParse_AddEnumToClass(ClassInfo *info, EnumInfo *item);
void vtkParse_AddConstantToClass(ClassInfo *info, ValueInfo *item);
void vtkParse_AddVariableToClass(ClassInfo *info, ValueInfo *item);
void vtkParse_AddTypedefToClass(ClassInfo *info, ValueInfo *item);
void vtkParse_AddUsingToClass(ClassInfo *info, UsingInfo *item);
void vtkParse_AddNamespaceToNamespace(NamespaceInfo *info,NamespaceInfo *item);
void vtkParse_AddClassToNamespace(NamespaceInfo *info, ClassInfo *item);
void vtkParse_AddFunctionToNamespace(NamespaceInfo *info, FunctionInfo *item);
void vtkParse_AddEnumToNamespace(NamespaceInfo *info, EnumInfo *item);
void vtkParse_AddConstantToNamespace(NamespaceInfo *info, ValueInfo *item);
void vtkParse_AddVariableToNamespace(NamespaceInfo *info, ValueInfo *item);
void vtkParse_AddTypedefToNamespace(NamespaceInfo *info, ValueInfo *item);
void vtkParse_AddUsingToNamespace(NamespaceInfo *info, UsingInfo *item);
void vtkParse_AddArgumentToFunction(FunctionInfo *info, ValueInfo *item);
void vtkParse_AddArgumentToTemplate(TemplateArgs *info, TemplateArg *item);
/*@}*/

/**
 * Add default constructors to a class if they do not already exist
 */
void vtkParse_AddDefaultConstructors(ClassInfo *data);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif

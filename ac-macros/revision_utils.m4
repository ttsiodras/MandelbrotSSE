# Automake macro to set the revision utils

# Usage: AM_REVISION_UTILS([directory_name]).
# Directory name must be a litteral string and not the result of a
# command.

AC_DEFUN([AM_REVISION_UTILS],
[
# Take the current SVN revision

AC_MSG_CHECKING(whether this is a SCM working copy)

temp_scm_output=`(cd $srcdir; git rev-parse --short HEAD)`

if test x"${temp_scm_output}" = x""; then
   # This means we are NOT working on a repository copy, the value of
   # SCM_REVISION will not be taken into account.

   SCM_REVISION=""
   AC_MSG_RESULT(no.)
else
   SCM_REVISION="r${temp_scm_output}"
   AC_MSG_RESULT(yes: ${SCM_REVISION}.)
fi

AC_SUBST(SCM_REVISION)

# Take the configure full date

AC_MSG_CHECKING(the configuration full date)

temp_config_date=`LANG=C date +"%A %d %B %Y, %H:%M:%S"`

if test x"${temp_config_date}" = x""; then
   LAST_CONFIG_DATE=""
   AC_MSG_RESULT(empty date.)
else
   LAST_CONFIG_DATE="${temp_config_date}"
   AC_MSG_RESULT(${LAST_CONFIG_DATE}.)
fi

AC_SUBST(LAST_CONFIG_DATE)

# Take the configure year

AC_MSG_CHECKING(the configuration year)

temp_config_year=`LANG=C date +"%Y"`

if test x"${temp_config_year}" = x""; then
   LAST_CONFIG_YEAR="2007"
   AC_MSG_RESULT(no year found, fall into default: ${LAST_CONFIG_YEAR}.)
else
   LAST_CONFIG_YEAR="${temp_config_year}"
   AC_MSG_RESULT(${LAST_CONFIG_YEAR})
fi

AC_SUBST(LAST_CONFIG_YEAR)
])

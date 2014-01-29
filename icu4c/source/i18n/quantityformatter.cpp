/*
******************************************************************************
* Copyright (C) 2014, International Business Machines
* Corporation and others.  All Rights Reserved.
******************************************************************************
* quantityformatter.cpp
*/
#include "quantityformatter.h"
#include "template.h"
#include "uassert.h"
#include "unicode/unistr.h"
#include "unicode/decimfmt.h"
#include "cstring.h"
#include "plurrule_impl.h"
#include "unicode/plurrule.h"
#include "charstr.h"
#include "unicode/fmtable.h"

#define LENGTHOF(array) (int32_t)(sizeof(array) / sizeof((array)[0]))

U_NAMESPACE_BEGIN

// other must always be first.
static const char * const gPluralForms[] = {
        "other", "zero", "one", "two", "few", "many"};

static int32_t getPluralIndex(const char *pluralForm) {
    int32_t len = LENGTHOF(gPluralForms);
    for (int32_t i = 0; i < len; ++i) {
        if (uprv_strcmp(pluralForm, gPluralForms[i]) == 0) {
            return i;
        }
    }
    return -1;
}

QuantityFormatter::QuantityFormatter() {
    for (int32_t i = 0; i < LENGTHOF(templates); ++i) {
        templates[i] = NULL;
    }
}

QuantityFormatter::QuantityFormatter(const QuantityFormatter &other) {
    for (int32_t i = 0; i < LENGTHOF(templates); ++i) {
        if (other.templates[i] == NULL) {
            templates[i] = NULL;
        } else {
            templates[i] = new Template(*other.templates[i]);
        }
    }
}

QuantityFormatter &QuantityFormatter::operator=(
        const QuantityFormatter& other) {
    if (this == &other) {
        return *this;
    }
    for (int32_t i = 0; i < LENGTHOF(templates); ++i) {
        delete templates[i];
        if (other.templates[i] == NULL) {
            templates[i] = NULL;
        } else {
            templates[i] = new Template(*other.templates[i]);
        }
    }
    return *this;
}

QuantityFormatter::~QuantityFormatter() {
    for (int32_t i = 0; i < LENGTHOF(templates); ++i) {
        delete templates[i];
    }
}

void QuantityFormatter::reset() {
    for (int32_t i = 0; i < LENGTHOF(templates); ++i) {
        delete templates[i];
        templates[i] = NULL;
    }
}

UBool QuantityFormatter::add(
        const char *variant,
        const UnicodeString &rawPattern,
        UErrorCode &status) {
    if (U_FAILURE(status)) {
        return FALSE;
    }
    int32_t pluralIndex = getPluralIndex(variant);
    if (pluralIndex == -1) {
        status = U_ILLEGAL_ARGUMENT_ERROR;
        return FALSE;
    }
    Template *newTemplate = new Template(rawPattern);
    if (newTemplate == NULL) {
        status = U_MEMORY_ALLOCATION_ERROR;
        return FALSE;
    }
    if (newTemplate->getPlaceholderCount() > 1) {
        delete newTemplate;
        status = U_ILLEGAL_ARGUMENT_ERROR;
        return FALSE;
    }
    delete templates[pluralIndex];
    templates[pluralIndex] = newTemplate;
    return TRUE;
}

UnicodeString &QuantityFormatter::format(
            const Formattable& quantity,
            const NumberFormat &fmt,
            const PluralRules &rules,
            UnicodeString &appendTo,
            UErrorCode &status) const {
    if (U_FAILURE(status)) {
        return appendTo;
    }
    UnicodeString count;
    const DecimalFormat *decFmt = dynamic_cast<const DecimalFormat *>(&fmt);
    if (decFmt != NULL) {
        FixedDecimal fd = decFmt->getFixedDecimal(quantity, status);
        if (U_FAILURE(status)) {
            return appendTo;
        }
        count = rules.select(fd);
    } else {
        if (quantity.getType() == Formattable::kDouble) {
            count = rules.select(quantity.getDouble());
        } else if (quantity.getType() == Formattable::kLong) {
            count = rules.select(quantity.getLong());
        } else if (quantity.getType() == Formattable::kInt64) {
            count = rules.select((double) quantity.getInt64());
        } else {
            status = U_ILLEGAL_ARGUMENT_ERROR;
            return appendTo;
        }
    }
    CharString buffer;
    buffer.appendInvariantChars(count, status);
    if (U_FAILURE(status)) {
        return appendTo;
    }
    int32_t pluralIndex = getPluralIndex(buffer.data());
    if (pluralIndex == -1) {
        pluralIndex = 0;
    }
    const Template *pattern = templates[pluralIndex];
    if (pattern == NULL) {
        pattern = templates[0];
    }
    if (pattern == NULL) {
        status = U_INVALID_STATE_ERROR;
        return appendTo;
    }
    UnicodeString formattedNumber;
    FieldPosition pos(0);
    fmt.format(quantity, formattedNumber, pos, status);
    UnicodeString *params[] = {&formattedNumber};
    return pattern->evaluate(params, LENGTHOF(params), appendTo, status);
}

U_NAMESPACE_END
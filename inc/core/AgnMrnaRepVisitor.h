/**

Copyright (c) 2010-2014, Daniel S. Standage and CONTRIBUTORS

The AEGeAn Toolkit is distributed under the ISC License. See
the 'LICENSE' file in the AEGeAn source code distribution or
online at https://github.com/standage/AEGeAn/blob/master/LICENSE.

**/
#ifndef AEGEAN_MRNA_REP_VISITOR
#define AEGEAN_MRNA_REP_VISITOR

#include "extended/node_stream_api.h"
#include "AgnUnitTest.h"

/**
 * @class AgnMrnaRepVisitor
 *
 * Implements the GenomeTools ``GtNodeVisitor`` interface. This is a node
 * visitor used for filtering out all but the longest mRNA (as measured by CDS
 * length) from alternatively spliced genes.
 */
typedef struct AgnMrnaRepVisitor AgnMrnaRepVisitor;

/**
 * @function Constructor for a node stream based on this node visitor.
 */
GtNodeStream* agn_mrna_rep_stream_new(GtNodeStream *in, FILE *mapstream);

/**
 * @function Constructor for the node visitor. If `mapstream` is not NULL, each
 * gene/mRNA rep pair will be written to `mapstream`.
 */
GtNodeVisitor *agn_mrna_rep_visitor_new(FILE *mapstream);

/**
 * @function By default, the representative mRNA for each gene will be reported.
 * Use this function to specify an alternative top-level feature to gene (such
 * as locus).
 */
void agn_mrna_rep_visitor_set_parent_type(AgnMrnaRepVisitor *v,
                                          const char *type);

/**
 * @function Run unit tests for this class. Returns true if all tests passed.
 */
bool agn_mrna_rep_visitor_unit_test(AgnUnitTest *test);

#endif

#include <math.h>
#include <string.h>
#include "extended/feature_node.h"
#include "AgnGeneLocus.h"
#include "AgnGtExtensions.h"
#include "AgnUtils.h"

//----------------------------------------------------------------------------//
// Data structure definition
//----------------------------------------------------------------------------//
struct AgnGeneLocus
{
  AgnLoc locus;
  GtDlist *genes;
  GtHashmap *refr_genes;
  GtHashmap *pred_genes;
  GtArray *refr_cliques;
  GtArray *pred_cliques;
  GtArray *clique_pairs;
  bool     clique_pairs_formed;
  GtArray *reported_pairs;
  GtArray *unique_refr_cliques;
  GtArray *unique_pred_cliques;
};


//----------------------------------------------------------------------------//
// Prototypes for private method(s)
//----------------------------------------------------------------------------//

/**
 * Update this locus' start and end coordinates based on the gene being merged.
 *
 * @param[out] locus    the locus
 * @param[in]  gene     gene that was just merged with the locus
 */
void agn_gene_locus_update_range(AgnGeneLocus *locus, GtFeatureNode *gene);


//----------------------------------------------------------------------------//
// Method implementations
//----------------------------------------------------------------------------//
void agn_gene_locus_add_gene(AgnGeneLocus *locus, GtFeatureNode *gene)
{
  gt_dlist_add(locus->genes, gene);
  agn_gene_locus_update_range(locus, gene);
}

void agn_gene_locus_add_pred_gene(AgnGeneLocus *locus, GtFeatureNode *gene)
{
  agn_gene_locus_add_gene(locus, gene);
  gt_hashmap_add(locus->pred_genes, gene, gene);
}

void agn_gene_locus_add_refr_gene(AgnGeneLocus *locus, GtFeatureNode *gene)
{
  agn_gene_locus_add_gene(locus, gene);
  gt_hashmap_add(locus->refr_genes, gene, gene);
}

int agn_gene_locus_array_compare(const void *p1, const void *p2)
{
  AgnGeneLocus *l1 = *(AgnGeneLocus **)p1;
  AgnGeneLocus *l2 = *(AgnGeneLocus **)p2;
  GtRange l1r = l1->locus.range;
  GtRange l2r = l2->locus.range;

  bool equal = l1r.start == l2r.start && l1r.end == l2r.end;
  if(equal)
    return 0;

  bool l1startfirst = l1r.start <  l2r.start;
  bool l1endfirst   = l1r.start == l2r.start && l1r.end < l2r.end;  
  if(l1startfirst || l1endfirst)
    return -1;
  
  return 1;
}

void agn_gene_locus_delete(AgnGeneLocus *locus)
{
  GtDlistelem *current;
  for(current = gt_dlist_first(locus->genes);
      current != NULL;
      current = gt_dlistelem_next(current))
  {
    GtGenomeNode *gn = gt_dlistelem_get_data(current);
    gt_genome_node_delete(gn);
  }
  gt_dlist_delete(locus->genes);
  gt_hashmap_delete(locus->refr_genes);
  gt_hashmap_delete(locus->pred_genes);

  if(locus->refr_cliques != NULL)
  {
    while(gt_array_size(locus->refr_cliques) > 0)
    {
      AgnTranscriptClique *clique = *(AgnTranscriptClique **)
                                              gt_array_pop(locus->refr_cliques);
      agn_transcript_clique_delete(clique);
    }
    gt_array_delete(locus->refr_cliques);
  }
  if(locus->pred_cliques != NULL)
  {
    while(gt_array_size(locus->pred_cliques) > 0)
    {
      AgnTranscriptClique *clique = *(AgnTranscriptClique **)
                                              gt_array_pop(locus->pred_cliques);
      agn_transcript_clique_delete(clique);
    }
    gt_array_delete(locus->pred_cliques);
  }

  if(locus->clique_pairs != NULL)
  {
    while(gt_array_size(locus->clique_pairs) > 0)
    {
      AgnCliquePair *pair = *(AgnCliquePair**)gt_array_pop(locus->clique_pairs);
      agn_clique_pair_delete(pair);
    }
    gt_array_delete(locus->clique_pairs);
  }

  if(locus->reported_pairs != NULL)
    gt_array_delete(locus->reported_pairs);
  if(locus->unique_refr_cliques != NULL)
    gt_array_delete(locus->unique_refr_cliques);
  if(locus->unique_pred_cliques != NULL)
    gt_array_delete(locus->unique_pred_cliques);

  gt_free(locus->locus.seqid);
  gt_free(locus);
  locus = NULL;
}

bool agn_gene_locus_filter(AgnGeneLocus *locus, AgnCompareFilters *filters)
{
  if(filters == NULL)
    return false;

  // Ignore all filter configuration settings set to 0

  // Filter by locus length
  unsigned long length = agn_gene_locus_get_length(locus);
  if(filters->LocusLengthUpperLimit > 0)
  {
    if(length > filters->LocusLengthUpperLimit)
      return true;
  }
  if(filters->LocusLengthLowerLimit > 0)
  {
    if(length < filters->LocusLengthLowerLimit)
      return true;
  }

  // Filter by reference gene models
  unsigned long num_refr_genes = agn_gene_locus_num_refr_genes(locus);
  if(filters->MinReferenceGeneModels > 0)
  {
    if(num_refr_genes < filters->MinReferenceGeneModels)
      return true;
  }
  if(filters->MaxReferenceGeneModels > 0)
  {
    if(num_refr_genes > filters->MaxReferenceGeneModels)
      return true;
  }

  // Filter by prediction gene models
  unsigned long num_pred_genes = agn_gene_locus_num_pred_genes(locus);
  if(filters->MinPredictionGeneModels > 0)
  {
    if(num_pred_genes < filters->MinPredictionGeneModels)
      return true;
  }
  if(filters->MaxPredictionGeneModels > 0)
  {
    if(num_pred_genes > filters->MaxPredictionGeneModels)
      return true;
  }

  // Filter by reference transcript models
  unsigned long nrefr_transcripts = agn_gene_locus_num_refr_transcripts(locus);
  if(filters->MinReferenceTranscriptModels > 0)
  {
    if(nrefr_transcripts < filters->MinReferenceTranscriptModels)
      return true;
  }
  if(filters->MaxReferenceTranscriptModels > 0)
  {
    if(nrefr_transcripts > filters->MaxReferenceTranscriptModels)
      return true;
  }

  // Filter by prediction transcript models
  unsigned long npred_transcripts = agn_gene_locus_num_pred_transcripts(locus);
  if(filters->MinPredictionTranscriptModels > 0)
  {
    if(npred_transcripts < filters->MinPredictionTranscriptModels)
      return true;
  }
  if(filters->MaxPredictionTranscriptModels > 0)
  {
    if(npred_transcripts > filters->MaxPredictionTranscriptModels)
      return true;
  }

  // Filter by number of transcripts per gene model
  GtArray *refr_genes = agn_gene_locus_get_refr_genes(locus);
  if(filters->MinTranscriptsPerReferenceGeneModel > 0)
  {
    unsigned long i;
    bool success = false;
    for(i = 0; i < gt_array_size(refr_genes); i++)
    {
      GtFeatureNode *gene = gt_array_get(refr_genes, i);
      if(agn_gt_feature_node_num_transcripts(gene) >=
         filters->MinTranscriptsPerReferenceGeneModel)
      {
        success = true;
        break;
      }
    }
    if(!success)
    {
      gt_array_delete(refr_genes);
      return true;
    }
  }
  if(filters->MaxTranscriptsPerReferenceGeneModel > 0)
  {
    unsigned long i;
    bool success = false;
    for(i = 0; i < gt_array_size(refr_genes); i++)
    {
      GtFeatureNode *gene = gt_array_get(refr_genes, i);
      if(agn_gt_feature_node_num_transcripts(gene) <=
         filters->MaxTranscriptsPerReferenceGeneModel)
      {
        success = true;
        break;
      }
    }
    if(!success)
    {
      gt_array_delete(refr_genes);
      return true;
    }
  }
  gt_array_delete(refr_genes);
  GtArray *pred_genes = agn_gene_locus_get_pred_genes(locus);
  if(filters->MinTranscriptsPerPredictionGeneModel > 0)
  {
    unsigned long i;
    bool success = false;
    for(i = 0; i < gt_array_size(pred_genes); i++)
    {
      GtFeatureNode *gene = gt_array_get(pred_genes, i);
      if(agn_gt_feature_node_num_transcripts(gene) >=
         filters->MinTranscriptsPerPredictionGeneModel)
      {
        success = true;
        break;
      }
    }
    if(!success)
    {
      gt_array_delete(pred_genes);
      return true;
    }
  }
  if(filters->MaxTranscriptsPerPredictionGeneModel > 0)
  {
    unsigned long i;
    bool success = false;
    for(i = 0; i < gt_array_size(pred_genes); i++)
    {
      GtFeatureNode *gene = gt_array_get(pred_genes, i);
      if(agn_gt_feature_node_num_transcripts(gene) <=
         filters->MaxTranscriptsPerPredictionGeneModel)
      {
        success = true;
        break;
      }
    }
    if(!success)
    {
      gt_array_delete(refr_genes);
      return true;
    }
  }
  gt_array_delete(pred_genes);

  // Filter by reference exons
  unsigned long num_refr_exons = agn_gene_locus_num_refr_exons(locus);
  if(filters->MinReferenceExons > 0)
  {
    if(num_refr_exons < filters->MinReferenceExons)
      return true;
  }
  if(filters->MaxReferenceExons > 0)
  {
    if(num_refr_exons > filters->MaxReferenceExons)
      return true;
  }

  // Filter by prediction exons
  unsigned long num_pred_exons = agn_gene_locus_num_pred_exons(locus);
  if(filters->MinPredictionExons > 0)
  {
    if(num_pred_exons < filters->MinPredictionExons)
      return true;
  }
  if(filters->MaxPredictionExons > 0)
  {
    if(num_pred_exons > filters->MaxPredictionExons)
      return true;
  }

  // Filter by reference CDS length
  unsigned long refr_cds_length = agn_gene_locus_refr_cds_length(locus);
  if(filters->MinReferenceCDSLength > 0)
  {
    if(refr_cds_length < filters->MinReferenceCDSLength)
      return true;
  }
  if(filters->MaxReferenceCDSLength > 0)
  {
    if(refr_cds_length > filters->MaxReferenceCDSLength)
      return true;
  }

  // Filter by prediction CDS length
  unsigned long pred_cds_length = agn_gene_locus_pred_cds_length(locus);
  if(filters->MinPredictionCDSLength > 0)
  {
    if(pred_cds_length < filters->MinPredictionCDSLength)
      return true;
  }
  if(filters->MaxPredictionCDSLength > 0)
  {
    if(pred_cds_length > filters->MaxPredictionCDSLength)
      return true;
  }

  return false;
}

GtArray *agn_gene_locus_find_best_pairs(AgnGeneLocus *locus)
{
  if(locus->refr_cliques == NULL || gt_array_size(locus->refr_cliques) == 0 ||
     locus->pred_cliques == NULL || gt_array_size(locus->pred_cliques) == 0)
  {
    return NULL;
  }

  if(locus->reported_pairs != NULL)
    return locus->reported_pairs;

  gt_array_sort(locus->clique_pairs,(GtCompare)agn_clique_pair_compare_reverse);
  GtHashmap *refr_cliques_acctd = gt_hashmap_new(GT_HASH_STRING, NULL, NULL);
  GtHashmap *pred_cliques_acctd = gt_hashmap_new(GT_HASH_STRING, NULL, NULL);
  locus->reported_pairs = gt_array_new( sizeof(AgnCliquePair *) );

  unsigned long i;
  for(i = 0; i < gt_array_size(locus->clique_pairs); i++)
  {
    AgnCliquePair *pair = *(AgnCliquePair **)
                                           gt_array_get(locus->clique_pairs, i);
    AgnTranscriptClique *rclique = agn_clique_pair_get_refr_clique(pair);
    AgnTranscriptClique *pclique = agn_clique_pair_get_pred_clique(pair);
    if(!agn_transcript_clique_has_id_in_hash(rclique, refr_cliques_acctd) &&
       !agn_transcript_clique_has_id_in_hash(pclique, pred_cliques_acctd))
    {
      gt_array_add(locus->reported_pairs, pair);
      agn_transcript_clique_put_ids_in_hash(rclique, refr_cliques_acctd);
      agn_transcript_clique_put_ids_in_hash(pclique, pred_cliques_acctd);
    }
  }

  locus->unique_refr_cliques = gt_array_new( sizeof(AgnTranscriptClique *) );
  for(i = 0; i < gt_array_size(locus->refr_cliques); i++)
  {
    AgnTranscriptClique *refr_clique = *(AgnTranscriptClique **)
                                           gt_array_get(locus->refr_cliques, i);
    if(!agn_transcript_clique_has_id_in_hash(refr_clique, refr_cliques_acctd))
    {
      gt_array_add(locus->unique_refr_cliques, refr_clique);
      agn_transcript_clique_put_ids_in_hash(refr_clique, refr_cliques_acctd);
    }
  }

  locus->unique_pred_cliques = gt_array_new( sizeof(AgnTranscriptClique *) );
  for(i = 0; i < gt_array_size(locus->pred_cliques); i++)
  {
    AgnTranscriptClique *pred_clique = *(AgnTranscriptClique **)
                                           gt_array_get(locus->pred_cliques, i);
    if(!agn_transcript_clique_has_id_in_hash(pred_clique, pred_cliques_acctd))
    {
      gt_array_add(locus->unique_pred_cliques, pred_clique);
      agn_transcript_clique_put_ids_in_hash(pred_clique, pred_cliques_acctd);
    }
  }

  gt_hashmap_delete(refr_cliques_acctd);
  gt_hashmap_delete(pred_cliques_acctd);
  return locus->reported_pairs;
}

GtArray* agn_gene_locus_get_clique_pairs(AgnGeneLocus *locus,
                                         unsigned int trans_per_locus)
{
  GtArray *refr_trans, *pred_trans;
  GtDlistelem *elem;

  // No need to do this multiple times
  if(locus->clique_pairs_formed)
    return locus->clique_pairs;

  // Grab reference transcripts
  refr_trans = gt_array_new( sizeof(GtFeatureNode *) );
  pred_trans = gt_array_new( sizeof(GtFeatureNode *) );
  for(elem = gt_dlist_first(locus->genes);
      elem != NULL;
      elem = gt_dlistelem_next(elem))
  {
    GtFeatureNode *gene = (GtFeatureNode *)gt_dlistelem_get_data(elem);
    GtFeatureNodeIterator *iter = gt_feature_node_iterator_new_direct(gene);
    GtFeatureNode *feature;
    for(feature = gt_feature_node_iterator_next(iter);
        feature != NULL;
        feature = gt_feature_node_iterator_next(iter))
    {
      if(agn_gt_feature_node_is_mrna_feature(feature))
      {
        if(gt_hashmap_get(locus->refr_genes, gene) != NULL)
          gt_array_add(refr_trans, feature);
        else if(gt_hashmap_get(locus->pred_genes, gene) != NULL)
          gt_array_add(pred_trans, feature);
      }
    }
    gt_feature_node_iterator_delete(iter);
  }

  // Compute maximal transcript cliques
  bool have_refr_trans = gt_array_size(refr_trans) > 0;
  bool refr_trans_reasonable =
  (
    gt_array_size(refr_trans) <= trans_per_locus ||
    trans_per_locus == 0
  );
  if(have_refr_trans && refr_trans_reasonable)
  {
    locus->refr_cliques = agn_enumerate_feature_cliques(refr_trans);
  }
  bool have_pred_trans = gt_array_size(pred_trans) > 0;
  bool pred_trans_reasonable =
  (
    gt_array_size(pred_trans) <= trans_per_locus ||
    trans_per_locus == 0
  );
  if(have_pred_trans && pred_trans_reasonable)
  {
    locus->pred_cliques = agn_enumerate_feature_cliques(pred_trans);
  }

  // Pairs require both reference and prediction cliques
  if(!have_refr_trans || !have_pred_trans)
  {
    locus->clique_pairs_formed = true;
    //if(options->debug)
    if(false)
    {
      fprintf
      (
        stderr,
        "debug: skipping locus %s[%lu, %lu] with %lu reference transcripts and"
        " %lu prediction transcripts (must have at least 1 transcript for"
        " both)\n",
        locus->locus.seqid,
        locus->locus.range.start,
        locus->locus.range.end,
        gt_array_size(refr_trans),
        gt_array_size(pred_trans)
      );
    }
    gt_array_delete(refr_trans);
    gt_array_delete(pred_trans);
    return NULL;
  }

  if(!refr_trans_reasonable || !pred_trans_reasonable)
  {
    locus->clique_pairs_formed = true;
    //if(options->debug)
    if(false)
    {
      fprintf
      (
        stderr,
        "debug: skipping locus %s[%lu, %lu] with %lu reference transcripts and"
        " %lu prediction transcripts (exceeds reasonable limit of %u)\n",
        locus->locus.seqid,
        locus->locus.range.start,
        locus->locus.range.end,
        gt_array_size(refr_trans),
        gt_array_size(pred_trans),
        trans_per_locus
      );
    }
    gt_array_delete(refr_trans);
    gt_array_delete(pred_trans);
    return NULL;
  }

  gt_array_delete(refr_trans);
  gt_array_delete(pred_trans);

  // Form all possible pairs of reference and prediction cliques
  GtArray *clique_pairs = gt_array_new( sizeof(AgnCliquePair *) );
  unsigned long i,j;
  for(i = 0; i < gt_array_size(locus->refr_cliques); i++)
  {
    AgnTranscriptClique *refr_clique, *pred_clique;
    refr_clique = *(AgnTranscriptClique **)gt_array_get(locus->refr_cliques, i);
    for(j = 0; j < gt_array_size(locus->pred_cliques); j++)
    {
      pred_clique = *(AgnTranscriptClique**)gt_array_get(locus->pred_cliques,j);
      AgnCliquePair *pair = agn_clique_pair_new(locus->locus.seqid,
                                                refr_clique, pred_clique,
                                                &locus->locus.range);
      gt_array_add(clique_pairs, pair);
    }
  }

  locus->clique_pairs = clique_pairs;
  locus->clique_pairs_formed = true;
  return locus->clique_pairs;
}

unsigned long agn_gene_locus_get_end(AgnGeneLocus *locus)
{
  return locus->locus.range.end;
}

unsigned long agn_gene_locus_get_length(AgnGeneLocus *locus)
{
  return gt_range_length(&locus->locus.range);
}

AgnCliquePair* agn_gene_locus_get_optimal_clique_pair(AgnGeneLocus *locus,
                                              AgnTranscriptClique *refr_clique)
{
  AgnCliquePair *bestpair = NULL;
  unsigned long i;

  for(i = 0; i < gt_array_size(locus->clique_pairs); i++)
  {
    AgnCliquePair *currentpair;
    currentpair = *(AgnCliquePair **)gt_array_get(locus->clique_pairs, i);
    if(refr_clique == agn_clique_pair_get_refr_clique(currentpair))
    {
      if(bestpair == NULL ||
         agn_clique_pair_compare_direct(bestpair, currentpair) == -1)
      {
        bestpair = currentpair;
      }
    }
  }

  return bestpair;
}

GtArray *agn_gene_locus_get_pred_genes(AgnGeneLocus *locus)
{
  GtArray *genes = gt_array_new( sizeof(GtFeatureNode *) );
  GtDlistelem *elem;
  for(elem = gt_dlist_first(locus->genes);
      elem != NULL;
      elem = gt_dlistelem_next(elem))
  {
    GtFeatureNode *gene = gt_dlistelem_get_data(elem);
    if(gt_hashmap_get(locus->pred_genes, gene) != NULL)
      gt_array_add(genes, gene);
  }
  gt_array_sort(genes, (GtCompare)agn_gt_genome_node_compare);
  return genes;
}

GtArray *agn_gene_locus_get_pred_gene_ids(AgnGeneLocus *locus)
{
  GtArray *ids = gt_array_new( sizeof(char *) );
  GtDlistelem *elem;
  for(elem = gt_dlist_first(locus->genes);
      elem != NULL;
      elem = gt_dlistelem_next(elem))
  {
    GtFeatureNode *gene = gt_dlistelem_get_data(elem);
    if(gt_hashmap_get(locus->pred_genes, gene) != NULL)
    {
      const char *id = gt_feature_node_get_attribute(gene, "ID");
      gt_array_add(ids, id);
    }
  }
  gt_array_sort(ids, (GtCompare)agn_string_compare);
  return ids;
}

double agn_gene_locus_get_pred_splice_complexity(AgnGeneLocus *locus)
{
  GtArray *pred_trans = agn_gene_locus_get_pred_transcripts(locus);
  double sc = agn_calc_splice_complexity(pred_trans);
  gt_array_delete(pred_trans);
  return sc;
}

GtArray *agn_gene_locus_get_pred_transcripts(AgnGeneLocus *locus)
{
  GtArray *transcripts = gt_array_new( sizeof(GtFeatureNode *) );
  GtDlistelem *elem;
  for(elem = gt_dlist_first(locus->genes);
      elem != NULL;
      elem = gt_dlistelem_next(elem))
  {
    GtFeatureNode *gene = gt_dlistelem_get_data(elem);
    GtFeatureNodeIterator *iter = gt_feature_node_iterator_new_direct(gene);
    GtFeatureNode *feature;

    for(feature = gt_feature_node_iterator_next(iter);
        feature != NULL;
        feature = gt_feature_node_iterator_next(iter))
    {
      if(agn_gt_feature_node_is_mrna_feature(feature) &&
         gt_hashmap_get(locus->pred_genes, gene))
      {
        gt_array_add(transcripts, feature);
      }
    }
    gt_feature_node_iterator_delete(iter);
  }
  gt_array_sort(transcripts, (GtCompare)agn_gt_genome_node_compare);
  return transcripts;
}

GtArray *agn_gene_locus_get_pred_transcript_ids(AgnGeneLocus *locus)
{
  GtArray *ids = gt_array_new( sizeof(char *) );
  GtDlistelem *elem;
  for(elem = gt_dlist_first(locus->genes);
      elem != NULL;
      elem = gt_dlistelem_next(elem) )
  {
    GtFeatureNode *gene = gt_dlistelem_get_data(elem);
    GtFeatureNodeIterator *iter = gt_feature_node_iterator_new_direct(gene);
    GtFeatureNode *feature;

    for(feature = gt_feature_node_iterator_next(iter);
        feature != NULL;
        feature = gt_feature_node_iterator_next(iter))
    {
      if(agn_gt_feature_node_is_mrna_feature(feature) &&
         gt_hashmap_get(locus->pred_genes, gene))
      {
        const char *id = gt_feature_node_get_attribute(feature, "ID");
        gt_array_add(ids, id);
      }
    }
    gt_feature_node_iterator_delete(iter);
  }
  gt_array_sort(ids, (GtCompare)agn_string_compare);
  return ids;
}

GtArray *agn_gene_locus_get_refr_genes(AgnGeneLocus *locus)
{
  GtArray *genes = gt_array_new( sizeof(GtFeatureNode *) );
  GtDlistelem *elem;
  for(elem = gt_dlist_first(locus->genes);
      elem != NULL;
      elem = gt_dlistelem_next(elem))
  {
    GtFeatureNode *gene = gt_dlistelem_get_data(elem);
    if(gt_hashmap_get(locus->refr_genes, gene))
      gt_array_add(genes, gene);
  }
  gt_array_sort(genes, (GtCompare)gt_genome_node_compare);
  return genes;
}

GtArray *agn_gene_locus_get_refr_gene_ids(AgnGeneLocus *locus)
{
  GtArray *ids = gt_array_new( sizeof(char *) );
  GtDlistelem *elem;
  for(elem = gt_dlist_first(locus->genes);
      elem != NULL;
      elem = gt_dlistelem_next(elem))
  {
    GtFeatureNode *gene = gt_dlistelem_get_data(elem);
    if(gt_hashmap_get(locus->refr_genes, gene))
    {
      const char *id = gt_feature_node_get_attribute(gene, "ID");
      gt_array_add(ids, id);
    }
  }
  gt_array_sort(ids, (GtCompare)agn_string_compare);
  return ids;
}

double agn_gene_locus_get_refr_splice_complexity(AgnGeneLocus *locus)
{
  GtArray *refr_trans = agn_gene_locus_get_pred_transcripts(locus);
  double sc = agn_calc_splice_complexity(refr_trans);
  gt_array_delete(refr_trans);
  return sc;
}

GtArray *agn_gene_locus_get_refr_transcripts(AgnGeneLocus *locus)
{
  GtArray *transcripts = gt_array_new( sizeof(GtFeatureNode *) );
  GtDlistelem *elem;
  for(elem = gt_dlist_first(locus->genes);
      elem != NULL;
      elem = gt_dlistelem_next(elem))
  {
    GtFeatureNode *gene = gt_dlistelem_get_data(elem);
    GtFeatureNodeIterator *iter = gt_feature_node_iterator_new_direct(gene);
    GtFeatureNode *feature;

    for(feature = gt_feature_node_iterator_next(iter);
        feature != NULL;
        feature = gt_feature_node_iterator_next(iter))
    {
      if(agn_gt_feature_node_is_mrna_feature(feature) &&
         gt_hashmap_get(locus->refr_genes, gene))
      {
        gt_array_add(transcripts, feature);
      }
    }
    gt_feature_node_iterator_delete(iter);
  }
  gt_array_sort(transcripts, (GtCompare)gt_genome_node_compare);
  return transcripts;
}

GtArray *agn_gene_locus_get_refr_transcript_ids(AgnGeneLocus *locus)
{
  GtArray *ids = gt_array_new( sizeof(char *) );
  GtDlistelem *elem;
  for(elem = gt_dlist_first(locus->genes);
      elem != NULL;
      elem = gt_dlistelem_next(elem))
  {
    GtFeatureNode *gene = gt_dlistelem_get_data(elem);
    GtFeatureNodeIterator *iter = gt_feature_node_iterator_new_direct(gene);
    GtFeatureNode *feature;

    for(feature = gt_feature_node_iterator_next(iter);
        feature != NULL;
        feature = gt_feature_node_iterator_next(iter))
    {
      if(agn_gt_feature_node_is_mrna_feature(feature) &&
         gt_hashmap_get(locus->refr_genes, gene))
      {
        const char *id = gt_feature_node_get_attribute(feature, "ID");
        gt_array_add(ids, id);
      }
    }
    gt_feature_node_iterator_delete(iter);
  }
  gt_array_sort(ids, (GtCompare)agn_string_compare);
  return ids;
}

const char* agn_gene_locus_get_seqid(AgnGeneLocus *locus)
{
  return locus->locus.seqid;
}

unsigned long agn_gene_locus_get_start(AgnGeneLocus *locus)
{
  return locus->locus.range.start;
}

GtArray *agn_gene_locus_get_unique_pred_cliques(AgnGeneLocus *locus)
{
  return locus->unique_pred_cliques;
}

GtArray *agn_gene_locus_get_unique_refr_cliques(AgnGeneLocus *locus)
{
  return locus->unique_refr_cliques;
}

bool agn_gene_locus_is_complex(AgnGeneLocus *locus)
{
  return gt_array_size(locus->refr_cliques) > 1 ||
         gt_array_size(locus->pred_cliques) > 1;
}

AgnGeneLocus* agn_gene_locus_new(const char *seqid)
{
  AgnGeneLocus *locus = (AgnGeneLocus *)gt_malloc(sizeof(AgnGeneLocus));
  
  locus->locus.seqid = gt_cstr_dup(seqid);
  locus->locus.range.start = 0;
  locus->locus.range.end = 0;
  locus->genes = gt_dlist_new( (GtCompare)gt_genome_node_cmp );
  locus->refr_genes = gt_hashmap_new(GT_HASH_DIRECT, NULL, NULL);
  locus->pred_genes = gt_hashmap_new(GT_HASH_DIRECT, NULL, NULL);
  locus->clique_pairs = NULL;
  locus->clique_pairs_formed = false;
  locus->refr_cliques = NULL;
  locus->pred_cliques = NULL;
  locus->reported_pairs = NULL;
  locus->unique_refr_cliques = NULL;
  locus->unique_pred_cliques = NULL;

  return locus;
}

unsigned long agn_gene_locus_num_pred_exons(AgnGeneLocus *locus)
{
  GtDlistelem *elem;
  unsigned long exon_count = 0;
  for(elem = gt_dlist_first(locus->genes);
      elem != NULL;
      elem = gt_dlistelem_next(elem))
  {
    GtFeatureNode *gene = (GtFeatureNode *)gt_dlistelem_get_data(elem);
    if(gt_hashmap_get(locus->pred_genes, gene) != NULL)
    {
      GtFeatureNodeIterator *iter = gt_feature_node_iterator_new(gene);
      GtFeatureNode *feature;
      for(feature = gt_feature_node_iterator_next(iter);
          feature != NULL;
          feature = gt_feature_node_iterator_next(iter))
      {
        if(agn_gt_feature_node_is_exon_feature(feature))
          exon_count++;
      }
      gt_feature_node_iterator_delete(iter);
    }
  }
  return exon_count;
}

unsigned long agn_gene_locus_num_pred_genes(AgnGeneLocus *locus)
{
  GtDlistelem *elem;
  unsigned long count = 0;
  for(elem = gt_dlist_first(locus->genes);
      elem != NULL;
      elem = gt_dlistelem_next(elem))
  {
    GtFeatureNode *gene = (GtFeatureNode *)gt_dlistelem_get_data(elem);
    if(gt_hashmap_get(locus->pred_genes, gene) != NULL)
      count++;
  }
  return count;
}

unsigned long agn_gene_locus_num_pred_transcripts(AgnGeneLocus *locus)
{
  GtDlistelem *elem;

  unsigned long transcript_count = 0;
  for(elem = gt_dlist_first(locus->genes);
      elem != NULL;
      elem = gt_dlistelem_next(elem))
  {
    GtFeatureNode *gene = (GtFeatureNode *)gt_dlistelem_get_data(elem);
    GtFeatureNodeIterator *iter = gt_feature_node_iterator_new_direct(gene);
    GtFeatureNode *feature;

    for(feature = gt_feature_node_iterator_next(iter);
        feature != NULL;
        feature = gt_feature_node_iterator_next(iter))
    {
      if(agn_gt_feature_node_is_mrna_feature(feature) &&
         gt_hashmap_get(locus->pred_genes, gene) != NULL)
      {
        transcript_count++;
      }
    }
    gt_feature_node_iterator_delete(iter);
  }
  return transcript_count;
}

unsigned long agn_gene_locus_num_refr_exons(AgnGeneLocus *locus)
{
  GtDlistelem *elem;
  unsigned long exon_count = 0;
  for(elem = gt_dlist_first(locus->genes);
      elem != NULL;
      elem = gt_dlistelem_next(elem))
  {
    GtFeatureNode *gene = (GtFeatureNode *)gt_dlistelem_get_data(elem);
    if(gt_hashmap_get(locus->refr_genes, gene) != NULL)
    {
      GtFeatureNodeIterator *iter = gt_feature_node_iterator_new(gene);
      GtFeatureNode *feature;
      for(feature = gt_feature_node_iterator_next(iter);
          feature != NULL;
          feature = gt_feature_node_iterator_next(iter))
      {
        if(agn_gt_feature_node_is_exon_feature(feature))
          exon_count++;
      }
      gt_feature_node_iterator_delete(iter);
    }
  }
  return exon_count;
}

unsigned long agn_gene_locus_num_refr_genes(AgnGeneLocus *locus)
{
  GtDlistelem *elem;
  unsigned long count = 0;
  for(elem = gt_dlist_first(locus->genes);
      elem != NULL;
      elem = gt_dlistelem_next(elem))
  {
    GtFeatureNode *gene = (GtFeatureNode *)gt_dlistelem_get_data(elem);
    if(gt_hashmap_get(locus->refr_genes, gene) != NULL)
      count++;
  }
  return count;
}

unsigned long agn_gene_locus_num_refr_transcripts(AgnGeneLocus *locus)
{
  GtDlistelem *elem;

  unsigned long transcript_count = 0;
  for(elem = gt_dlist_first(locus->genes);
      elem != NULL;
      elem = gt_dlistelem_next(elem))
  {
    GtFeatureNode *gene = (GtFeatureNode *)gt_dlistelem_get_data(elem);
    GtFeatureNodeIterator *iter = gt_feature_node_iterator_new_direct(gene);
    GtFeatureNode *feature;

    for(feature = gt_feature_node_iterator_next(iter);
        feature != NULL;
        feature = gt_feature_node_iterator_next(iter))
    {
      if(agn_gt_feature_node_is_mrna_feature(feature) &&
         gt_hashmap_get(locus->refr_genes, gene) != NULL)
      {
        transcript_count++;
      }
    }
    gt_feature_node_iterator_delete(iter);
  }
  return transcript_count;
}

unsigned long agn_gene_locus_pred_cds_length(AgnGeneLocus *locus)
{
  unsigned long length = 0, i;
  GtArray *transcripts = agn_gene_locus_get_pred_transcripts(locus);
  for(i = 0; i < gt_array_size(transcripts); i++)
  {
    GtFeatureNode *transcript = *(GtFeatureNode **)gt_array_get(transcripts, i);
    length = agn_gt_feature_node_cds_length(transcript);
  }
  gt_array_delete(transcripts);
  return length;
}

GtRange agn_gene_locus_range(AgnGeneLocus *locus)
{
  return locus->locus.range;
}

unsigned long agn_gene_locus_refr_cds_length(AgnGeneLocus *locus)
{
  unsigned long length = 0, i;
  GtArray *transcripts = agn_gene_locus_get_refr_transcripts(locus);
  for(i = 0; i < gt_array_size(transcripts); i++)
  {
    GtFeatureNode *transcript = *(GtFeatureNode **)gt_array_get(transcripts, i);
    length = agn_gt_feature_node_cds_length(transcript);
  }
  gt_array_delete(transcripts);
  return length;
}

void agn_gene_locus_update_range(AgnGeneLocus *locus, GtFeatureNode *gene)
{
  GtRange gene_range = gt_genome_node_get_range((GtGenomeNode *)gene);
  if(locus->locus.range.start == 0 && locus->locus.range.end == 0)
    locus->locus.range = gene_range;
  else
    locus->locus.range = gt_range_join(&locus->locus.range, &gene_range);
}

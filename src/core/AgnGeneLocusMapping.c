/**

Copyright (c) 2010-2014, Daniel S. Standage and CONTRIBUTORS

The AEGeAn Toolkit is distributed under the ISC License. See
the 'LICENSE' file in the AEGeAn source code distribution or
online at https://github.com/standage/AEGeAn/blob/master/LICENSE.

**/

#include <string.h>
#include "AgnGeneLocusMapping.h"
#include "core/array_api.h"
#include "core/file_api.h"

//------------------------------------------------------------------------------
// Data structure definition
//------------------------------------------------------------------------------

struct AgnGeneLocusMapping
{
  GtStr *filename;
  FILE *file;
  GtHashmap *mapping;
};

//------------------------------------------------------------------------------
// Prototypes for private functions
//------------------------------------------------------------------------------

typedef struct
{
  const char *locusid;
  GtStrArray *geneids;
} GetGeneIdsData;

/**
 * Callback function: collect all gene IDs associated with the given locus ID.
 */
int map_visit_get_geneids(void *key, void *value, void *data, GtError *error);

/**
 * Callback function: print gene-locus mapping.
 */
int map_visit_print(void *key, void *value, void *data, GtError *error);

//------------------------------------------------------------------------------
// Method implementations
//------------------------------------------------------------------------------

void agn_gene_locus_mapping_add(AgnGeneLocusMapping *map, AgnLocus *locus)
{
  GtArray *genes;
  const char *locusid;

  agn_assert(map && locus);
  locusid = gt_feature_node_get_attribute((GtFeatureNode *)locus, "ID");
  agn_assert(locusid && strcmp(locusid, "") != 0);

  genes = agn_locus_get_genes(locus);
  while(gt_array_size(genes) > 0)
  {
    GtFeatureNode *gene = gt_array_pop(genes);
    const char *geneid = gt_feature_node_get_attribute(gene, "ID");
    gt_hashmap_add(map->mapping, gt_cstr_dup(geneid), gt_str_new_cstr(locusid));
  }
  gt_array_delete(genes);
}

void agn_gene_locus_mapping_close(AgnGeneLocusMapping *map)
{
  agn_assert(map);
  if(map->file == NULL)
  {
    map->file = fopen(gt_str_get(map->filename), "w");
    if(map->file == NULL)
    {
      fprintf(stderr, "unable to open gene-locus map file '%s'\n",
              gt_str_get(map->filename));
      exit(1);
    }
  }

  GtError *error = gt_error_new();
  int result = gt_hashmap_foreach_in_key_order(map->mapping, map_visit_print,
                                               map->file, error);
  if(result)
  {
    fprintf(stderr, "error writing gene-locus mapping to disk: %s\n",
            gt_error_get(error));
    exit(1);
  }
  agn_gene_locus_mapping_delete(map);
}

void agn_gene_locus_mapping_delete(AgnGeneLocusMapping *map)
{
  agn_assert(map);
  gt_str_delete(map->filename);
  if(map->file)
    fclose(map->file);
  gt_hashmap_delete(map->mapping);
}

GtStrArray *
agn_gene_locus_mapping_get_geneids_for_locus(AgnGeneLocusMapping *map,
                                             const char *locusid,
                                             GtError *error)
{
  agn_assert(map && locusid && error);
  agn_assert(strcmp(locusid, "") != 0);

  GetGeneIdsData data;
  data.locusid = locusid;
  data.geneids = gt_str_array_new();
  gt_hashmap_foreach(map->mapping, map_visit_get_geneids, &data, error);
  return data.geneids;
}

GtStr *agn_gene_locus_mapping_get_locus(AgnGeneLocusMapping *map,
                                        const char *geneid)
{
  agn_assert(map && geneid);
  agn_assert(strcmp(geneid, "") != 0);
  GtStr *locusid = gt_hashmap_get(map->mapping, geneid);
  return gt_str_ref(locusid);
}

AgnGeneLocusMapping *agn_gene_locus_mapping_open(const char *filepath)
{
  AgnGeneLocusMapping *map = agn_gene_locus_mapping_new(filepath);

  FILE *mapfile = fopen(gt_str_get(map->filename), "r");
  if(mapfile == NULL)
  {
    fprintf(stderr, "unable to open gene-locus mapfile '%s'\n",
            gt_str_get(map->filename));
    exit(1);
  }

  char buffer[1024];
  while(fgets(buffer, 1024, mapfile) != NULL)
  {
    if(strlen(buffer) == 0)
      continue;
    char *geneid  = strtok(buffer, "\t\n");
    char *locusid = strtok(NULL,   "\t\n");
    agn_assert(geneid && locusid);
    gt_hashmap_add(map->mapping, gt_cstr_dup(geneid), gt_str_new_cstr(locusid));
  }
  fclose(mapfile);

  return map;
}

AgnGeneLocusMapping *agn_gene_locus_mapping_new(const char *filepath)
{
  AgnGeneLocusMapping *map = gt_malloc( sizeof(AgnGeneLocusMapping) );
  map->filename = gt_str_new_cstr(filepath);
  map->file = NULL;
  map->mapping = gt_hashmap_new(GT_HASH_STRING, (GtFree)gt_free_func,
                                (GtFree)gt_str_delete);
  return map;
}

GtStr *agn_gene_locus_mapping_unmap_gene(AgnGeneLocusMapping *map,
                                         const char *geneid)
{
  agn_assert(map && geneid);
  agn_assert(strcmp(geneid, "") != 0);
  GtStr *locusid = gt_hashmap_get(map->mapping, geneid);
  if(locusid == NULL)
    return NULL;
  
  gt_str_ref(locusid);
  gt_hashmap_remove(map->mapping, geneid);
  return locusid;
}

int map_visit_get_geneids(void *key, void *value, void *data, GtError *error)
{
  const char *geneid = key;
  GtStr *locusid = value;
  GetGeneIdsData *dat = data;
  if(strcmp(gt_str_get(locusid), dat->locusid) == 0)
    gt_str_array_add_cstr(dat->geneids, geneid);
  return 0;
}

int map_visit_print(void *key, void *value, void *data, GtError *error)
{
  const char *geneid = key;
  GtStr *locusid = value;
  FILE *outstream = data;
  fprintf(outstream, "%s\t%s\n", geneid, gt_str_get(locusid));
  return 0;
}
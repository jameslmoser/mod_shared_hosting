/* Module Configuration Hints
MODULE-DEFINITION-START
Name: shared_hosting_module
MODULE-DEFINITION-END
*/

#include "apr.h"
#include "apr_strings.h"
#include "apr_hooks.h"
#include "apr_lib.h"

#define APR_WANT_STRFUNC
#include "apr_want.h"

#include "httpd.h"
#include "http_config.h"
#include "http_core.h"
#include "http_request.h"  /* for ap_hook_translate_name */

#if !defined(WIN32) && !defined(OS2) && !defined(BEOS) && !defined(NETWARE)
#define HAVE_UNIX_SUEXEC
#endif

#ifdef HAVE_UNIX_SUEXEC
#include "unixd.h"        /* Contains the suexec_identity hook used on Unix */
#endif

module AP_MODULE_DECLARE_DATA shared_hosting_module;

// This is a per server configuration for mod_shared_hosting options
typedef struct {
	const char *alias;
	apr_array_header_t *maps;
} vsalias_t;

typedef struct {
	apr_array_header_t *vdroots;
	apr_array_header_t *vsaliases;
} shared_hosting_conf_t;

static void *shared_hosting_create_server_config(apr_pool_t *p, server_rec *s) {
	shared_hosting_conf_t *conf;
	
	conf = (shared_hosting_conf_t *) apr_pcalloc(p, sizeof(shared_hosting_conf_t));
	conf->vdroots =  apr_array_make(p,4, sizeof(const char *));
	conf->vsaliases = apr_array_make(p,2, sizeof(vsalias_t));

	return conf;
}

static int check_map(const char *p) {
	while (*p != '\0') {
		if (*p++ != '%')
			continue;
		if (*p == '-')
			++p;
		if (apr_isdigit(*p))
			++p;
		else
			return 1;
		if (*p == '+')
			++p;
		if (*p != '.')
			continue;
		++p;
		if (*p == '-')
			++p;
		if (apr_isdigit(*p))
			++p;
		else
			return 1;
		if (*p == '+')
			++p;
	}
	return 0;
}

static const char *shared_hosting_virtual_document_roots(cmd_parms *cmd, void *mconfig, const char *map) {
	shared_hosting_conf_t *conf;

	if( check_map(map) != 0)
		return "syntax error in format string";

	conf = (shared_hosting_conf_t *) ap_get_module_config(cmd->server->module_config, &shared_hosting_module);
	*(const char **) apr_array_push(conf->vdroots) = map;
	return NULL;
}

static const char *shared_hosting_virtual_scriptaliases(cmd_parms *cmd, void *mconfig, const char *alias, const char *map) {
	shared_hosting_conf_t *conf;
	vsalias_t *vsalias = NULL;
	int i;

	if (check_map(map) != 0)
		return "syntax error in format string";

	conf = (shared_hosting_conf_t *) ap_get_module_config(cmd->server->module_config, &shared_hosting_module);
	for (i = 0; i < conf->vsaliases->nelts; i++) {
		vsalias_t * tmp= &((vsalias_t *) (conf->vsaliases->elts))[i];
		if( strcmp(tmp->alias,alias)==0){
			vsalias=tmp;
			break;
		}
	}

	if (vsalias == NULL) {
		vsalias = (vsalias_t *) apr_array_push(conf->vsaliases);
		vsalias->alias = alias;
		vsalias->maps = apr_array_make(cmd->pool,0,sizeof(const char *));
	}

	*(const char **) apr_array_push(vsalias->maps) = map;
	return NULL;
}


static int vhost_alias_interpolate(request_rec *r,const char *map, const char *alias) {
	enum { MAXDOTS = 19 };
	const char *dots[MAXDOTS+1], *start, *end, *p;
	int ndots, N, M, Np, Mp, Nd, Md;
	char *dest, last;
    apr_finfo_t finfo;
#if APR_HAS_USER 		
    char *user, *group;
#endif

	const char *name = ap_get_server_name(r);
	char *docroot = (char *) apr_palloc(r->pool, sizeof( char[512] ));

	ndots = 0;
	dots[ndots++] = name - 1; /* slightly naughty */
	for (p = name; *p; ++p)
		if (*p == '.' && ndots < MAXDOTS)
			dots[ndots++] = p;
	dots[ndots] = p;

	dest = docroot;
	last = '\0';
	while (*map) {
		if (*map != '%') { /* normal characters */
			last = *dest++ = *map++;
			continue;
		}
		++map; /* we are in a format specifier */
		last = '\0'; /* can't be a slash */
		/* deal with %-N+.-M+ -- syntax is already checked */
		N = M = 0;   /* value */
		Np = Mp = 0; /* is there a plus? */
		Nd = Md = 0; /* is there a dash? */
		if (*map == '-') ++map, Nd = 1;
		N = *map++ - '0';
		if (*map == '+') ++map, Np = 1;
		if (*map == '.') {
			++map;
			if (*map == '-') {
				++map, Md = 1;
			}
			M = *map++ - '0';
			if (*map == '+') {
				++map, Mp = 1;
			}
		}
		/* note that N and M are one-based indices, not zero-based */
		start = dots[0]+1; /* ptr to the first character */
		end = dots[ndots]; /* ptr to the character after the last one */
		if (N != 0) {
			if (N > ndots) {
				start = "_";
				end = start+1;
			} else if (!Nd) {
				start = dots[N-1]+1;
				if (!Np) {
					end = dots[N];
				}
			} else {
				if (!Np) {
					start = dots[ndots-N]+1;
				}
				end = dots[ndots-N+1];
			}
		}
		if (M != 0) {
			if (M > end - start) {
				start = "_";
				end = start+1;
			} else if (!Md) {
				start = start+M-1;
				if (!Mp) {
					end = start+1;
				}
			} else {
				if (!Mp) {
					start = end-M;
				}
				end = end-M+1;
			}
		}
		for (p = start; p < end; ++p)
			*dest++ = apr_tolower(*p);
	}

	if(last=='/')
		*dest--;
	*dest='\0';
	
	if (apr_stat(&finfo, docroot, APR_FINFO_NORM, r->pool) == APR_SUCCESS){
		if (r->filename == NULL) {
				const char *uri = r->uri;
				if (alias) {
					p = strstr(uri, alias);
					p += strlen(alias);
					while(uri != p)
						++uri;

					while(*uri != '/')
						--uri;

					r->handler = "cgi-script";
					apr_table_setn(r->notes, "alias-forced-type", r->handler);
				}

				r->filename = apr_pstrcat(r->pool, docroot, uri, NULL);
		}		
		apr_table_setn(r->notes, "VIRTUAL_DOCUMENT_ROOT", docroot);

		return 0;
	}
	return 1;
}

static int shared_hosting_translate(request_rec *r){
	int i, num_vdrs;
	char **vdrs_ptr;
 	shared_hosting_conf_t *conf;

	r->filename = NULL;
	conf = (shared_hosting_conf_t *) ap_get_module_config(r->server->module_config, &shared_hosting_module);
	
	for (i=0; i < conf->vsaliases->nelts; i++) {
		vsalias_t *vsalias = &((vsalias_t *) (conf->vsaliases->elts))[i];
		if (
			strncmp(r->uri, vsalias->alias, strlen(vsalias->alias)) == 0 ||
			strncmp(r->uri +1,vsalias->alias, strlen(vsalias->alias)) == 0
		) {
			vdrs_ptr = (char **) vsalias->maps->elts;
			num_vdrs = vsalias->maps->nelts;
			for (; num_vdrs; ++vdrs_ptr, --num_vdrs) {
				const char *map  = *vdrs_ptr;
				if (vhost_alias_interpolate(r, map, vsalias->alias) == 0)
					break;
			}
			break;
		}
	}

	vdrs_ptr = (char **) conf->vdroots->elts;
	num_vdrs = conf->vdroots->nelts;
	for (; num_vdrs; ++vdrs_ptr, --num_vdrs) {
		const char *map  = *vdrs_ptr;
		if (vhost_alias_interpolate(r, map, NULL) == 0)
			break;
	}	

	if (r->filename)
		return OK;
		
	return DECLINED;
}

#ifdef HAVE_UNIX_SUEXEC
static ap_unix_identity_t *shared_hosting_get_suexec_id_doer(const request_rec *r)
{
    ap_unix_identity_t *ugid = NULL;
    const char *docroot;
    apr_finfo_t finfo;

    docroot = apr_table_get(r->notes, "VIRTUAL_DOCUMENT_ROOT");
    if (docroot == NULL) {
        return NULL;
    }

    if (apr_stat(&finfo, docroot, APR_FINFO_NORM, r->pool) == APR_SUCCESS){
        if ((ugid = apr_palloc(r->pool, sizeof(*ugid))) == NULL) {
            return NULL;
        }
        ugid->uid = finfo.user;
        ugid->gid = finfo.group;
        ugid->userdir = 0;
    }
    
    return ugid;
}
#endif /* HAVE_UNIX_SUEXEC */

static int shared_hosting_init(apr_pool_t *pconf, apr_pool_t *plog, apr_pool_t *ptemp, server_rec *s){
	ap_add_version_component(pconf, "mod_shared_hosting/2.0");
	return OK;
}

static void register_hooks(apr_pool_t *p)
{
    static const char * const aszPre[] = { "mod_alias.c", "mod_userdir.c", NULL };
	
	ap_hook_post_config(shared_hosting_init, NULL, NULL, APR_HOOK_MIDDLE);
	ap_hook_translate_name(shared_hosting_translate, aszPre, NULL, APR_HOOK_MIDDLE);
#ifdef HAVE_UNIX_SUEXEC
    ap_hook_get_suexec_identity(shared_hosting_get_suexec_id_doer,NULL,NULL,APR_HOOK_FIRST);
#endif
}

static const command_rec shared_hosting_commands[] =
{
    AP_INIT_ITERATE(
        "VirtualDocumentRoots",
        shared_hosting_virtual_document_roots,
        NULL,
        RSRC_CONF,
        "Virtual Document Roots (returns first match found)"
    ),
    AP_INIT_ITERATE2(
        "VirtualScriptAliases",
        shared_hosting_virtual_scriptaliases,
        NULL,
        RSRC_CONF,
        "Virtual Script Alias (returns first pair match found)"
    ),
    { NULL }
};


module AP_MODULE_DECLARE_DATA shared_hosting_module =
{
    STANDARD20_MODULE_STUFF,
    NULL,							/* dir config creater */
    NULL,							/* dir merger --- default is to override */
    shared_hosting_create_server_config,	    /* server config */
    NULL,							/* merge server configs */
    shared_hosting_commands,				    /* command apr_table_t */
    register_hooks					/* register hooks */
};

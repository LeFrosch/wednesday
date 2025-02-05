URL_TEMPLATE = "https://github.com/mortie/snow/archive/refs/tags/v%s.tar.gz"
PREFIX_TEMPLATE = "snow-%s/snow"

def _impl(ctx):
    version = ctx.attr.version

    ctx.download_and_extract(
        stripPrefix = PREFIX_TEMPLATE % version,
        url = [URL_TEMPLATE % version],
        sha256 = ctx.attr.sha256,
    )

    ctx.symlink(ctx.attr._build_file, "BUILD")


snow_repo = repository_rule(
    implementation = _impl,
    attrs = {
        "version": attr.string(mandatory = True),
        "sha256": attr.string(mandatory = True),
        "_build_file": attr.label(
            default = ":BUILD.tmpl",
            allow_files = True,
        )
    },
)

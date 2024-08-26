void render(struct context* context, int num_meshes, const struct mesh** const meshes) {
    float bbox[4];
    int x0, x1, y0, y1;

    for (int i = 0; i < num_meshes; ++i) {
        const struct mesh* const mesh = meshes[i];
        const int* vi = mesh->face_vertex_indices;
        const int* sti = mesh->st.indices;

        for (int j = 0; j < mesh->num_triangles; ++j, vi += 3, sti += 3) {
            struct point3f p0 = mesh->points[vi[0]];
            struct point3f p1 = mesh->points[vi[1]];
            struct point3f p2 = mesh->points[vi[2]];

            persp_divide(&p0, context->znear);
            persp_divide(&p1, context->znear);
            persp_divide(&p2, context->znear);
            to_raster(context->screen_coordinates, context->extent, &p0);
            to_raster(context->screen_coordinates, context->extent, &p1);
            to_raster(context->screen_coordinates, context->extent, &p2);

            tri_bbox(&p0, &p1, &p2, bbox);

            if (bbox[0] > context->extent.width - 1 || bbox[2] < 0 || bbox[1] > context->extent.height - 1 || bbox[3] < 0)
                continue;

            x0 = MAX(0, (int)bbox[0]);
            y0 = MAX(0, (int)bbox[1]);
            x1 = MIN(context->extent.width - 1, (int)bbox[2]);
            y1 = MIN(context->extent.height - 1, (int)bbox[3]);

            struct texcoord2f st0 = mesh->st.coords[sti[0]];
            struct texcoord2f st1 = mesh->st.coords[sti[1]];
            struct texcoord2f st2 = mesh->st.coords[sti[2]];

            st0.s /= p0.z, st0.t /= p0.z;
            st1.s /= p1.z, st1.t /= p1.z;
            st2.s /= p2.z, st2.t /= p2.z;

            rasterize(x0, y0, x1, y1, &p0, &p1, &p2, &st0, &st1, &st2, mesh, context);
        }
    }
}

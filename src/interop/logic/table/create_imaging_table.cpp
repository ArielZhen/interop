/** Logic to populate the imaging table
 *
 *  @file
 *  @date  7/20/16
 *  @version 1.0
 *  @copyright GNU Public License.
 */
#include "interop/logic/table/create_imaging_table.h"

#include "interop/logic/table/table_util.h"
#include "interop/logic/summary/map_cycle_to_read.h"
#include "interop/logic/table/create_imaging_table_columns.h"
#include "interop/logic/table/table_populator.h"
#include "interop/logic/metric/q_metric.h"

namespace illumina { namespace interop { namespace logic { namespace table
{
    /** Populate the imaging table with a by cycle InterOp metric set
     *
     * @param beg iterator to start of the metric set
     * @param end iterator to end of the metric set
     * @param q20_idx index of the q20 value
     * @param q30_idx index of the q30_idxindex value
     * @param naming_method tile naming method enum
     * @param cycle_to_read map cycle to read/cycle within read
     * @param columns vector of table columns
     * @param index_map metric id to the row
     * @param column_count number of data columns including sub columns
     * @param data_beg iterator to start of table data
     * @param data_end iterator to end of table data
     */
    template<typename InputIterator, typename OutputIterator, typename Id_t>
    void populate_imaging_table_data_by_cycle(InputIterator beg,
                                              InputIterator end,
                                              const size_t q20_idx,
                                              const size_t q30_idx,
                                              const constants::tile_naming_method naming_method,
                                              const summary::read_cycle_vector_t& cycle_to_read,
                                              const std::vector<size_t>& columns,
                                              std::map<Id_t, size_t>& index_map,
                                              const size_t column_count,
                                              OutputIterator data_beg,
                                              OutputIterator data_end)
    {
        for(;beg != end;++beg)
        {
            const Id_t id = beg->id();
            typename std::map<Id_t, size_t>::iterator it = index_map.find(id);
            size_t row;
            const summary::read_cycle& read = cycle_to_read[beg->cycle()-1];
            if(it == index_map.end())
            {
                if((beg->cycle()-1) >= cycle_to_read.size())
                    INTEROP_THROW(model::index_out_of_bounds_exception, "Cycle exceeds total cycles from Reads in the RunInfo.xml");

                row = index_map.size();
                index_map[id]=row;
                INTEROP_ASSERTMSG(columns[model::table::ReadColumn] < static_cast<size_t>(model::table::ImagingColumnCount), columns[model::table::ReadColumn] );
                INTEROP_ASSERTMSG(columns[model::table::CycleWithinReadColumn] < static_cast<size_t>(model::table::ImagingColumnCount), columns[model::table::CycleWithinReadColumn] );
                INTEROP_ASSERTMSG(data_beg+row*column_count+columns[model::table::ReadColumn] < data_end, columns[model::table::ReadColumn] << " - " <<  row*column_count+columns[model::table::ReadColumn] << " < " << std::distance(data_beg, data_end));
                // TODO: Only populate Id once!
                table_populator::populate_id(*beg,
                                             read,
                                             q20_idx,
                                             q30_idx,
                                             naming_method,
                                             columns,
                                             data_beg+row*column_count,
                                             data_end);
            }
            else row = it->second;
            table_populator::populate(*beg,
                                      read.number,
                                      q20_idx,
                                      q30_idx,
                                      naming_method,
                                      columns,
                                      data_beg+row*column_count, data_end);
        }
    }
    /** Populate the imaging table with a by cycle InterOp metric set
     *
     * @param metrics InterOp metric set
     * @param q20_idx index of the q20 value
     * @param q30_idx index of the q30 value
     * @param naming_method tile naming method enum
     * @param cycle_to_read map cycle to read/cycle within read
     * @param columns vector of table columns
     * @param index_map metric id to the row
     * @param column_count number of data columns including sub columns
     * @param data_beg iterator to start of table data
     * @param data_end iterator to end of table data
     */
    template<class MetricSet, typename OutputIterator, typename Id_t>
    void populate_imaging_table_data_by_cycle(const MetricSet& metrics,
                                              const size_t q20_idx,
                                              const size_t q30_idx,
                                              const constants::tile_naming_method naming_method,
                                              const summary::read_cycle_vector_t& cycle_to_read,
                                              const std::vector<size_t>& columns,
                                              std::map<Id_t, size_t>& index_map,
                                              const size_t column_count,
                                              OutputIterator data_beg, OutputIterator data_end)
    {
        populate_imaging_table_data_by_cycle(metrics.begin(),
                                             metrics.end(),
                                             q20_idx,
                                             q30_idx,
                                             naming_method,
                                             cycle_to_read,
                                             columns,
                                             index_map,
                                             column_count,
                                             data_beg, data_end);
    }
    /** Zero out first column of every row
     *
     * @param
     */
    template<typename I>
    void zero_first_column(I beg, I end, size_t column_count)
    {
        for(;beg != end;beg+=column_count) *beg = 0;
    }
    /** Populate the imaging table with all the metrics in the run
     *
     * @param metrics collection of all run metrics
     * @param columns vector of table columns
     * @param data_beg iterator to start of table data
     * @param data_end iterator to end of table data
     */
    template<typename I>
    void create_imaging_table_data(const model::metrics::run_metrics& metrics,
                                   const std::vector<model::table::imaging_column>& columns,
                                   const std::map<model::metric_base::base_metric::id_t, size_t>& row_offset,
                                   I data_beg,
                                   I data_end)
    {
        typedef typename model::metrics::run_metrics::id_t id_t;
        const size_t column_count = columns.back().column_count();
        const constants::tile_naming_method naming_method = metrics.run_info().flowcell().naming_method();
        const size_t q20_idx = metric::index_for_q_value(metrics.get_set<model::metrics::q_metric>(), 20);
        const size_t q30_idx = metric::index_for_q_value(metrics.get_set<model::metrics::q_metric>(), 30);
        std::map<id_t, size_t> index_map;
        std::vector<size_t> cmap(model::table::ImagingColumnCount, std::numeric_limits<size_t>::max());
        for(size_t i=0;i<columns.size();++i) cmap[columns[i].id()] = columns[i].offset();
        summary::read_cycle_vector_t cycle_to_read;
        summary::map_read_to_cycle_number(metrics.run_info().reads().begin(),
                                          metrics.run_info().reads().end(),
                                          cycle_to_read);

        zero_first_column(data_beg, data_beg+column_count*row_offset.size(), column_count);
        populate_imaging_table_data_by_cycle(metrics.get_set<model::metrics::extraction_metric>(),
                                             q20_idx,
                                             q30_idx,
                                             naming_method,
                                             cycle_to_read,
                                             cmap,
                                             index_map,
                                             column_count,
                                             data_beg, data_end);
        populate_imaging_table_data_by_cycle(metrics.get_set<model::metrics::error_metric>(),
                                             q20_idx,
                                             q30_idx,
                                             naming_method,
                                             cycle_to_read,
                                             cmap,
                                             index_map,
                                             column_count,
                                             data_beg, data_end);
        populate_imaging_table_data_by_cycle(metrics.get_set<model::metrics::image_metric>(),
                                             q20_idx,
                                             q30_idx,
                                             naming_method,
                                             cycle_to_read,
                                             cmap,
                                             index_map,
                                             column_count,
                                             data_beg, data_end);
        populate_imaging_table_data_by_cycle(metrics.get_set<model::metrics::corrected_intensity_metric>(),
                                             q20_idx,
                                             q30_idx,
                                             naming_method,
                                             cycle_to_read,
                                             cmap,
                                             index_map,
                                             column_count,
                                             data_beg, data_end);
        populate_imaging_table_data_by_cycle(metrics.get_set<model::metrics::q_metric>(),
                                             q20_idx,
                                             q30_idx,
                                             naming_method,
                                             cycle_to_read,
                                             cmap,
                                             index_map,
                                             column_count,
                                             data_beg, data_end);
        const typename model::metrics::run_metrics::tile_metric_set_t& tile_metrics =
                metrics.get_set<model::metrics::tile_metric>();
        for(typename std::map<id_t, size_t>::const_iterator it = index_map.begin();it != index_map.end();++it)
        {
            const id_t tid = model::metric_base::base_cycle_metric::tile_hash_from_id(it->first);
            if (!tile_metrics.has_metric(tid)) continue;
            const id_t cycle = model::metric_base::base_cycle_metric::cycle_from_id(it->first);
            const size_t row = it->second;
            const summary::read_cycle& read = cycle_to_read[cycle-1];
            table_populator::populate(tile_metrics.get_metric(tid),
                                      read.number,
                                      q20_idx,
                                      q30_idx,
                                      naming_method,
                                      cmap,
                                      data_beg+row*column_count, data_end);
        }
    }
    /** Populate the imaging table with all the metrics in the run
     *
     * @param metrics collection of all run metrics
     * @param columns vector of table columns
     * @param data_beg iterator to start of table data
     * @param n number of cells in the data table
     */
    template<typename I>
    void populate_imaging_table_data_t(const model::metrics::run_metrics& metrics,
                                       const std::vector<model::table::imaging_column>& columns,
                                       const std::map<model::metric_base::base_metric::id_t, size_t>& row_offset,
                                     I data_beg, const size_t n)
    {
        create_imaging_table_data(metrics, columns, row_offset, data_beg, data_beg+n);
    }
    /** Populate the imaging table with all the metrics in the run
     *
     * @param metrics collection of all run metrics
     * @param columns vector of table columns
     * @param row_offset ordering for the rows
     * @param data_beg iterator to start of table data
     * @param n number of cells in the data table
     */
    void populate_imaging_table_data(const model::metrics::run_metrics& metrics,
                                     const std::vector<model::table::imaging_column>& columns,
                                     const std::map<model::metric_base::base_metric::id_t, size_t>& row_offset,
                                     float* data_beg,
                                     const size_t n)
    {
        create_imaging_table_data(metrics, columns, row_offset, data_beg, data_beg+n);
    }
    /** Count the number of rows in the imaging table and setup an ordering
     *
     * @param metrics collections of InterOp metric sets
     * @param row_offset ordering for the rows
     */
    void count_table_rows(const model::metrics::run_metrics& metrics,
                            std::map<model::metric_base::base_metric::id_t, size_t>& row_offset)
    {
        typedef model::metrics::run_metrics::cycle_metric_map_t cycle_metric_map_t;
        cycle_metric_map_t hash_set;
        metrics.populate_id_map(hash_set);
        row_offset.clear();
        std::vector<model::metric_base::base_metric::id_t> ids(hash_set.size());
        size_t row = 0;
        for(cycle_metric_map_t::const_iterator it = hash_set.begin();it != hash_set.end();++it,++row)
            ids[row]=it->first;
        std::sort(ids.begin(), ids.end());
        for(size_t i=0;i<ids.size();++i) row_offset[ids[i]]=i;
    }
    /** Count the total number of columns for the data table
     *
     * @param columns vector of table column descriptions
     * @return total number of columns including sub columns
     */
    size_t count_table_columns(const std::vector<model::table::imaging_column>& columns)
    {
        if(columns.empty()) return 0;
        return columns.back().column_count();
    }
    /** Create an imaging table from run metrics
     *
     * @param metrics source run metrics
     * @param table destination imaging table
     */
    void create_imaging_table(const model::metrics::run_metrics& metrics, model::table::imaging_table& table)
                                        throw(model::invalid_column_type, model::index_out_of_bounds_exception)
    {
        typedef model::table::imaging_table::column_vector_t column_vector_t;
        typedef model::table::imaging_table::data_vector_t data_vector_t;
        typedef std::map<model::metric_base::base_metric::id_t, size_t> row_offset_t;

        row_offset_t row_offset;
        column_vector_t columns;
        create_imaging_table_columns(metrics, columns);
        count_table_rows(metrics, row_offset);
        data_vector_t data(row_offset.size()*count_table_columns(columns), std::numeric_limits<float>::quiet_NaN());
        create_imaging_table_data(metrics, columns, row_offset, data.begin(), data.end());
        table.set_data(row_offset.size(), columns, data);
    }

}}}}

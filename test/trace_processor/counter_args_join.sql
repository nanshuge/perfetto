select ts, counters.name as counters_name, value, dur, ref, ref_type,
       arg_id, id, args.name as args_name,
       int_value as utid
from counters
inner join args on counters.arg_id = args.id
where ref = 1
limit 10;

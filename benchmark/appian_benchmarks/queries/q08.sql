select t4rp1 as g0, t5rp1 as g1, sum(creditCard_lastChargeAmount) as p0, min(t6rp1) as p1, sum(t3rp2) as p2 from CreditCardView cc left outer join ( select min(order_id) as t1rp1, creditCard_number as t1pk from CreditCardView cc left outer join OrderView o on cc.creditCard_number = o.order_creditCardNumber where order_slaProbability > 0.125 group by creditCard_number ) t1 on cc.creditCard_number = t1pk left outer join ( select sum(orderItem_weight) as t2rp1, creditCard_number as t2pk from CreditCardView cc left outer join OrderView o on cc.creditCard_number = o.order_creditCardNumber left outer join OrderItemView oi on o.order_id = oi.orderItem_orderId group by creditCard_number ) t2 on cc.creditCard_number = t2pk left outer join ( select min(address_zip) as t3rp1, sum(taxRecord_bracketThreshold) as t3rp2, creditCard_number as t3pk from CreditCardView cc left outer join OrderView o on cc.creditCard_number = o.order_creditCardNumber left outer join CustomerView c on o.order_customerId = c.customer_id left outer join AddressView a on c.customer_id = a.address_customerId left outer join TaxRecordView t on a.address_id = t.taxRecord_addressId group by creditCard_number ) t3 on cc.creditCard_number = t3pk left outer join ( select sum(product_price) as t4rp1, creditCard_number as t4pk from CreditCardView cc left outer join OrderView o on cc.creditCard_number = o.order_creditCardNumber left outer join OrderItemView oi on o.order_id = oi.orderItem_orderId left outer join ProductView p on oi.orderItem_productId = p.product_id where orderItem_weight < 25.0 group by creditCard_number ) t4 on cc.creditCard_number = t4pk left outer join ( select sum(category_regulationProbability) as t5rp1, creditCard_number as t5pk from CreditCardView cc left outer join OrderView o on cc.creditCard_number = o.order_creditCardNumber left outer join OrderItemView oi on o.order_id = oi.orderItem_orderId left outer join ProductView p on oi.orderItem_productId = p.product_id left outer join CategoryView ca on p.product_categoryName = ca.category_name group by creditCard_number ) t5 on cc.creditCard_number = t5pk left outer join ( select min(product_inventoryLastOrderedOn) as t6rp1, creditCard_number as t6pk from CreditCardView cc left outer join OrderView o on cc.creditCard_number = o.order_creditCardNumber left outer join OrderItemView oi on o.order_id = oi.orderItem_orderId left outer join ProductView p on oi.orderItem_productId = p.product_id left outer join CategoryView ca on p.product_categoryName = ca.category_name where product_price < 200.0 group by creditCard_number ) t6 on cc.creditCard_number = t6pk where t1rp1 > 10000 or t2rp1 > 15 or t3rp1 > 20200 group by t4rp1, t5rp1 order by p0, p1, p2 limit 500;